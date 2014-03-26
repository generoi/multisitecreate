<?php

/**
 * Form for creating a multisite instance.
 */
function multisitecreate_create_form($form, &$form_state) {
  $form['subdomain'] = array(
    '#type' => 'textfield',
    '#title' => t('Subdomain'),
    '#description' => t('The subdomain which the new site will use.'),
    '#suffix' => multisitecreate_get_domain(''),
    '#element_validate' => array('_multisitecreate_subdomain_validate'),
  );
  $profile = variable_get('multisitecreate_profilename', 'standard');
  $form['profile'] = array(
    '#type' => 'hidden',
    '#value' => $profile,
  );
  include_once DRUPAL_ROOT . '/includes/install.core.inc';
  $locale_options = array();
  $locales = install_find_locales($profile);
  // If there are more than 1 language enabled, let the user choose which
  // language to use.
  if (count($locales) > 1) {
    $languages = language_list();
    foreach ($locales as $locale) {
      $language = $languages[$locale->langcode]->native;
      $locale_options[$locale->langcode] = $language;
    }
    $form['locale'] = array(
      '#type' => 'select',
      '#title' => t('Locale'),
      '#options' => $locale_options,
    );
  }
  // If there's only one language, do not display a select list.
  else {
    $form['locale'] = array(
      '#type' => 'hidden',
      '#value' => $locales[0]->name,
    );
  }

  $form['actions']['#type'] = 'actions';
  $form['actions']['save'] = array(
    '#type' => 'submit',
    '#value' => t('Create'),
  );
  $form['actions']['cancel'] = array(
    '#type' => 'link',
    '#title' => t('Cancel'),
    '#href' => isset($_GET['destination']) ? $_GET['destination'] : 'admin/people',
  );
  return $form;
}

/**
 * Scaffold the multisite directory, its settings.php file and finally redirect
 * to the sites install.php script.
 */
function multisitecreate_create_form_submit($form, &$form_state) {
  $values = $form_state['values'];
  $subdomain = $values['subdomain'];
  $new_domain = multisitecreate_get_domain($subdomain);
  $multisite_path = DRUPAL_ROOT . "/sites/$new_domain";
  $settings_path = "$multisite_path/settings.php";
  $settings_default = variable_get('multisitecreate_default_settings', 'sites/default/default.settings.php');

  if (($settings_contents = file_get_contents($settings_default)) === FALSE) {
    drupal_set_message(t('Could not read the default settings file at @path.', array('@path' => $settings_default)), 'error');
    return;
  }

  if (!@drupal_mkdir($multisite_path)) {
    drupal_set_message(t('Could not create the multisite directory at @path.', array('@path' => $multisite_path)), 'error');
    return;
  }


  // Replace the database name and generate a hash salt.
  // @see install_settings_form_submit().
  // We cannot use drupal_rewrite_settings() as it's locked to the current
  // working sites directory.
  $hash = VERSION < 7.24 ? drupal_hash_base64(drupal_random_bytes(55)) : drupal_random_key();
  $db_setup = variable_get('multisitecreate_db_setup', MULTISITECREATE_DB_SAME_PREFIX);
  // If a database is created, this name will be used
  $dbname = multisitecreate_get_db($new_domain);
  switch ($db_setup) {
    // Same database but prefix tables, add the subdomain as a prefix
    case MULTISITECREATE_DB_SAME_PREFIX:
      $settings_contents .= "\n\n\$databases['default']['default']['prefix'] = '${subdomain}_';";
      break;

    // The instance used some shared tables, add a condition which uses it's
    // own tables for installation (install fails if tables exist) and then
    // the shared tables on regular usage.
    case MULTISITECREATE_DB_SAME_SHARED:
    case MULTISITECREATE_DB_SEPARATE_SHARED:
      // Use table separator for same database and database separator for
      // separate database.
      if ($db_setup === MULTISITECREATE_DB_SEPARATE_SHARED) {
        $separator = '.';
        // When it's a separate db, use the generated db name instead of the
        // collition prone prefix subdomain.
        $subdomain = $dbname;
      }
      else {
        $separator = '_';
      }

      $settings_contents .= <<<EOT
// On installation and update runs separate tables are required.
if (preg_match('@^/(install|update)\.php@', request_uri(), \$matches) && !isset(\$install_done)) {
  \$databases['default']['default']['prefix'] = '${subdomain}${separator}';
}
// Once installed, use the shared tables.
else {
  \$databases['default']['default']['prefix']['default'] = '${subdomain}${separator}';
}
EOT;
      break;
    // Separate db (no shared tables) simply uses it's own database setting.
    case MULTISITECREATE_DB_SEPARATE:
      // do nothing, the database configuration is added later.
  }

  // Create the multisites database if setup requires it.
  if (in_array($db_setup, array(MULTISITECREATE_DB_SEPARATE, MULTISITECREATE_DB_SEPARATE_SHARED))) {
    // Append a database name setting.
    $settings_contents .= "\n\n\$databases['default']['default']['database'] = '$dbname';";
    db_query("CREATE DATABASE $dbname");
  }

  // Append a unique salt
  $settings_contents .= "\n\n\$drupal_hash_salt = '$hash';\n";

  if (file_put_contents($settings_path, $settings_contents) === FALSE) {
    drupal_set_message(t('Could not create the multisite settings file at @path.', array('@path' => $settings_path)), 'error');
    return;
  }

  global $user;
  $is_authenticated = $user->uid > 1;
  // If the user is logged in and is not the main admin user.
  if ($is_authenticated) {
    $sid = session_id();
    global $cookie_domain;
    setcookie('multisitecreate_sid', $sid, 0, '/', $cookie_domain);
    cache_set("create:$sid", $user, 'cache_multisitecreate', CACHE_TEMPORARY);
  }

  drupal_goto("http://$new_domain/install.php", array(
    'external' => TRUE,
    'query' => array(
      'locale' => $values['locale'],
      'profile' => $values['profile'],
      'create_user' => $is_authenticated ? 'false' : 'true',
    ),
  ));
}

/**
 * Make sure it's safe to create this multisite
 */
function multisitecreate_create_form_validate($form, &$form_state) {
  $directory = 'sites';
  if (!file_prepare_directory($sites)) {
    form_set_error(NULL, t('The sites directory is not writable'));
  }
}

/**
 * Validate a subdomain.
 */
function _multisitecreate_subdomain_validate($element, &$form_state) {
  $value = $element['#value'];
  if (!preg_match('/^[a-z]+[a-z\-_]*[a-z]+$/', $value)) {
    form_error($element, t('The subdomain can only use lowercased letters, dashes and underscores. It must also start and end with a letter.'));
  }
  if (in_array($value, array('www'))) {
    form_error($element, t('@value is a restricted subdomain.', array('@value' => $value)));
  }
  $new_domain = multisitecreate_get_domain($value);

  // If the multisites use their own database, check that it doesnt exist.
  switch (variable_get('multisitecreate_db_setup', MULTISITECREATE_DB_SAME_PREFIX)) {
    case MULTISITECREATE_DB_SEPARATE:
    case MULTISITECREATE_DB_SEPARATE_SHARED:
      $database = multisitecreate_get_db($new_domain);
      $exists = db_query('SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = :name', array(':name' => $database))->fetchField();
      if ($exists) {
        form_error($element, t('The database @name already exists', array('@name' => $database)));
      }
      break;
  }

  if (is_dir(DRUPAL_ROOT . "/sites/$new_domain")) {
    form_error($element, t('The subdomain already exists'));
  }
}

/**
 * Return the full domain name of a multisite based on its subdomain.
 */
function multisitecreate_get_domain($subdomain) {
  $domain = variable_get('multisitecreate_domain', '');
  if (empty($domain)) {
    $host = explode('.', $_SERVER['SERVER_NAME']);
    // Shift away leading www subdomain.
    if ($host[0] === 'www') {
      array_shift($host);
    }
    $domain = implode('.', $host);
  }
  drupal_alter('multisitecreate_domain', $subdomain, $domain);
  return "$subdomain.$domain";
}

/**
 * Return a safe database name for a domain.
 */
function multisitecreate_get_db($domain) {
  return preg_replace(array('/\./', '/\-/'), array('_', ''), $domain);
}

/**
 * General configuration form controlling multisite creation.
 */
function multisitecreate_admin_settings() {
  $form['multisitecreate_domain'] = array(
    '#type' => 'textfield',
    '#title' => t('Domain'),
    '#default_value' => variable_get('multisitecreate_domain', ''),
    '#description' => t('If left empty it wil default to the current site domain without the www subdomain. Other subdomains will be kept.'),
  );

  $form['multisitecreate_db_setup'] = array(
    '#type' => 'select',
    '#title' => t('Database setup'),
    '#default_value' => variable_get('multisitecreate_db_setup', MULTISITECREATE_DB_SAME_PREFIX),
    '#options' => array(
      MULTISITECREATE_DB_SAME_PREFIX => t('Same database with prefixed tables'),
      MULTISITECREATE_DB_SEPARATE => t('Create a new database for each install'),
      MULTISITECREATE_DB_SAME_SHARED => t('Same database with prefixed and shared tables'),
      MULTISITECREATE_DB_SEPARATE_SHARED => t('Create a new database for each install but share some tables.'),
    ),
  );

  include_once DRUPAL_ROOT . '/includes/install.core.inc';
  $profiles = install_find_profiles();
  $profiles = array_keys($profiles);
  $profiles = array_combine($profiles, $profiles);

  $form['multisitecreate_profilename'] = array(
    '#type' => 'select',
    '#title' => t('Installation profile'),
    '#description' => t('The installation profile to use when creating a multisite instance.'),
    '#default_value' => variable_get('multisitecreate_profilename', 'standard'),
    '#options' => $profiles,
  );

  $form['multisitecreate_default_settings'] = array(
    '#type' => 'textfield',
    '#title' => t('Settings file'),
    '#description' => t('Path to the settings.php file to use as a base for new multisites. This file should contain the database username and password prefilled! Only the database name will be replaced by the creation process.'),
    '#default_value' => variable_get('multisitecreate_default_settings', 'sites/default/default.settings.php'),
  );

  $form['multisitecreate_public_form'] = array(
    '#type' => 'checkbox',
    '#title' => t('Public create form'),
    '#description' => t('Add a separate !link, where users with the permission %permission can create sites (if the user is logged and is not the administrator that user will be used for the blog role).', array('!link' => l(t('creation form'), 'multisitecreate'), '%permission' => t('Create multisite'))),
    '#default_value' => variable_get('multisitecreate_public_form', FALSE),
  );

  return system_settings_form($form);
}
