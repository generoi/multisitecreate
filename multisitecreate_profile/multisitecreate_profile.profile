<?php

define('MULTISITECREATE_ADMIN_ROLE', 'administrator');

function multisitecreate_profile_install_tasks_alter(&$tasks, $install_state) {
}

function multisitecreate_profile_form_install_configure_form_alter(&$form, $form_state) {
  $form['admin_account']['account']['name']['#element_validate'][] = 'multisitecreate_profile_validate_name';
  $form['admin_account']['account']['mail']['#element_validate'][] = 'multisitecreate_profile_validate_mail';
  $form['#submit'][] = 'multisitecreate_profile_install_configure_form_submit';
}

function multisitecreate_profile_validate_name($element, &$form_state, $form) {
  $value = $element['#value'];
  $query = db_query('SELECT * FROM shared_users WHERE name = :name', array(':name' => $value));
  if ($query->rowCount() > 0) {
    form_error($element, t('The username is already taken.'));
  }
}

function multisitecreate_profile_validate_mail($element, &$form_state, $form) {
  $value = $element['#value'];
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
  $admin_role = user_role_load_by_name(MULTISITECREATE_ADMIN_ROLE);
  $account->roles = array($admin_role->rid);
  $account->uid = NULL;
  // Save the new account into the shared user table.
  user_save($account);
}
