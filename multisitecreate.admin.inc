<?php

/**
 * Form for creating a multisite instance.
 */
function multisitecreate_create_form($form, &$form_state) {
  $form['subdomain'] = array(
    '#type' => 'textfield',
    '#title' => t('Subdomain'),
    '#description' => t('The subdomain which the new site will use.'),
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
  foreach ($locales as $locale) {
    $locale_options[$locale->langcode] = $locale->langcode;
  }
  $form['locale'] = array(
    '#type' => 'select',
    '#title' => t('Locale'),
    '#options' => $locale_options,
  );
  // if (variable_get('multisitecreate_db_setup', MULTISITECREATE_DB_SHARED)) {
  //   $form['user'] = array(
  //     ''
  //   );
  // }

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
  switch (variable_get('multisitecreate_db_setup', MULTISITECREATE_DB_PREFIX)) {
    case MULTISITECREATE_DB_PREFIX:
      // Append a prefix setting
      $settings_contents .= "\n\n\$databases['default']['default']['prefix'] = '${subdomain}_';";
      break;
    case MULTISITECREATE_DB_SHARED:
      // Append a prefix setting
      $settings_contents .= <<<EOT
if (preg_match('@^/(install|update)\.php@', request_uri(), \$matches) && !isset(\$install_done)) {
  // On installation separate tables are required.
  \$databases['default']['default']['prefix'] = '${subdomain}_';
}
// Once installed, use the shared tables.
else {
  \$databases['default']['default']['prefix']['default'] = '${subdomain}_';
}
EOT;
      break;
    case MULTISITECREATE_DB_CREATE:
      // Append a database name setting.
      $dbname = multisitecreate_get_db($new_domain);
      $settings_contents .= "\n\n\$databases['default']['default']['database'] = '$dbname';";
      db_query("CREATE DATABASE $dbname");
      break;
  }

  // Append a unique salt
  $settings_contents .= "\n\n\$drupal_hash_salt = '$hash';\n";

  if (file_put_contents($settings_path, $settings_contents) === FALSE) {
    drupal_set_message(t('Could not create the multisite settings file at @path.', array('@path' => $settings_path)), 'error');
    return;
  }

  drupal_goto("http://$new_domain/install.php", array(
    'external' => TRUE,
    'query' => array(
      'locale' => $values['locale'],
      'profile' => $values['profile'],
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
  $database = multisitecreate_get_db($new_domain);
  $exists = db_query('SELECT SCHEMA_NAME FROM INFORMATION_SCHEMA.SCHEMATA WHERE SCHEMA_NAME = :name', array(':name' => $database))->fetchField();
  if ($exists) {
    form_error($element, t('The database @name already exists', array('@name' => $database)));
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
    '#default_value' => variable_get('multisitecreate_db_setup', MULTISITECREATE_DB_PREFIX),
    '#options' => array(
      MULTISITECREATE_DB_PREFIX => t('Same database with prefixed tables'),
      MULTISITECREATE_DB_CREATE => t('Create a new database for each install'),
      MULTISITECREATE_DB_SHARED => t('Same database with prefixed and shared tables'),
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
  return system_settings_form($form);
}