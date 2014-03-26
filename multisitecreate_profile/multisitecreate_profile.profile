<?php

function multisitecreate_profile_form_install_configure_form_alter(&$form, $form_state) {

  // Pre-populate the site name with the server name.
  $form['site_information']['site_name']['#default_value'] = $_SERVER['SERVER_NAME'];

  // Hide some messages
  drupal_get_messages('status');
  drupal_get_messages('warning');

  // If multisitecreate was initialized with a valid user that we can use for
  // admin account.
  if (!empty($_GET['create_user']) && $_GET['create_user'] == 'false') {
    $form['admin_account']['#access'] = FALSE;
    $form['admin_account']['account']['name']['#type'] = 'hidden';
    $form['admin_account']['account']['mail']['#type'] = 'hidden';
    $form['admin_account']['account']['pass']['#type'] = 'hidden';
    $form['admin_account']['account']['name']['#value'] = 'admin';
    $form['admin_account']['account']['mail']['#value'] = variable_get('multisitecreate_temp_email', 'admin@example.org');
    $form['admin_account']['account']['pass']['#value'] = user_password();
  }
  // If an administrator or anonymous user initalized the prcess, let them
  // create the admin account but validate the fields in the shared tables.
  else {
    $form['admin_account']['account']['name']['#element_validate'][] = 'multisitecreate_profile_validate_name';
    $form['admin_account']['account']['mail']['#element_validate'][] = 'multisitecreate_profile_validate_mail';
  }

  // Submit handler creating the account on the shared table.
  $form['#submit'][] = 'multisitecreate_profile_install_configure_form_submit';
}

function multisitecreate_profile_validate_name($element, &$form_state, $form) {
  $value = $element['#value'];
  // Shared users table, should be settin in the settings.php file
  $user_table = variable_get('multisitecreate_user_table', '{users}');
  $query = db_query("SELECT * FROM $user_table WHERE name = :name", array(':name' => $value));
  if ($query->rowCount() > 0) {
    form_error($element, t('The username is already taken.'));
  }
}

function multisitecreate_profile_validate_mail($element, &$form_state, $form) {
  $value = $element['#value'];
  // Shared users table, should be settin in the settings.php file
  $user_table = variable_get('multisitecreate_user_table', '{users}');
  $query = db_query("SELECT * FROM $user_table WHERE mail = :mail", array(':mail' => $value));
  if ($query->rowCount() > 0) {
    form_error($element, t('The e-mail address is already taken.'));
  }
}

function multisitecreate_profile_install_configure_form_submit($form, $form_state) {
  global $databases, $install_done;
  // Load the user account from the temporary user table.
  $account = clone user_load(1);
  // Remove the uid to create a new user in the shared table.
  $account->uid = NULL;

  // Mark the installation as done so settings.php picks up the prefixed tables.
  $install_done = TRUE;
  // Override the $databases array.
  include conf_path() . '/settings.php';
  // Close the current connection and reparse the $databases array.
  db_close();
  Database::parseConnectionInfo();

  // If the process was initialized by a user, use that as the multisites
  // account.
  if ($initial_user = _multisitecreate_profile_get_user()) {
    $account = $initial_user;
  }

  // The role for users of the new sites.
  $role = user_role_load_by_name(variable_get('multisitecreate_admin_role', 'administrator'));

  $account->roles = array($role->rid => $role);
  // Delete admin accounts paths as they use the same account name and therefore
  // take up the namespace user_save would use.
  path_delete(array('source' => 'user/1'));
  path_delete(array('source' => 'blog/1'));
  // Save the new account into the shared user table.
  user_save($account);

  // Login as the correct user.
  global $user;
  $user = $account;
  user_login_finalize();

  // Remove write access to the multisite instance.
  $multisite_path = conf_path(FALSE);
  $settings_path = "$multisite_path/settings.php";
  drupal_chmod($settings_path, 0444);
  drupal_chmod($multisite_path, 0555);
}

function _multisitecreate_profile_get_user() {
  $user = NULL;
  if (!empty($_COOKIE['multisitecreate_sid'])) {
    $sid = $_COOKIE['multisitecreate_sid'];
    $user = db_select('cache_multisitecreate', 'c')
      ->fields('c', array('data'))
      ->condition('cid', "create:$sid")
      ->execute()
      ->fetchField();

    $user = @unserialize($user);
    // cache tables do not reread settings.php so we query it manually.
    // $user = cache_get("create:$sid", 'cache_multisitecreate');
  }
  return $user;
}
