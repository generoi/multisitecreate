<?php

function multisitecreate_profile_form_install_configure_form_alter(&$form, $form_state) {
  $form['admin_account']['account']['name']['#element_validate'][] = 'multisitecreate_profile_validate_name';
  $form['admin_account']['account']['mail']['#element_validate'][] = 'multisitecreate_profile_validate_mail';
  $form['#submit'][] = 'multisitecreate_profile_install_configure_form_submit';
}

function multisitecreate_profile_validate_name($element, &$form_state, $form) {
  $value = $element['#value'];
  // @TODO configurable table
  $query = db_query('SELECT * FROM shared_users WHERE name = :name', array(':name' => $value));
  if ($query->rowCount() > 0) {
    form_error($element, t('The username is already taken.'));
  }
}

function multisitecreate_profile_validate_mail($element, &$form_state, $form) {
  $value = $element['#value'];
  // @TODO configurable table
  $query = db_query('SELECT * FROM shared_users WHERE mail = :mail', array(':mail' => $value));
  if ($query->rowCount() > 0) {
    form_error($element, t('The e-mail address is already taken.'));
  }
}

function multisitecreate_profile_install_configure_form_submit($form, $form_state) {
  global $databases;
  global $install_done;
  // Load the user account from the temporary user table.
  $account = user_load(1);
  // Mark the installation as done so settings.php picks up the prefixed tables.
  $install_done = TRUE;
  // Override the $databases array.
  include conf_path() . '/settings.php';
  // Close the current connection and reparse the $databases array.
  db_close();
  Database::parseConnectionInfo();
  // Set the account as a new user with administrator privileges.
  $admin_role = new stdClass();
  $admin_role->name = 'administrator';
  $admin_role->weight = 2;
  user_role_save($admin_role);
  // If you've already created a role in your profile.install, you can load it
  // like so.
  // $admin_role = user_role_load_by_name('administrator');
  $account->roles = array($admin_role->rid => $admin_role);
  $account->uid = NULL;
  // Delete admin accounts paths as they use the same account name and therefore
  // take up the namespace user_save would use.
  path_delete(array('source' => 'user/1'));
  path_delete(array('source' => 'blog/1'));
  // Save the new account into the shared user table.
  user_save($account);

  // Remove write access to the multisite instance.
  $multisite_path = conf_path(FALSE);
  $settings_path = "$multisite_path/settings.php";
  drupal_chmod($settings_path, 0444);
  drupal_chmod($multisite_path, 0555);
}
