Multisite Create
================

_Deprecated in favour of [Multisite](https://github.com/generoi/multisite)_

INSTALLATION
------------
1. Either symlink the multisitecreate_profile installation profile to profiles/
   or include the files in your own profile and use the functions provided for
   creating a shared user after installation.

2. Create a shared settings.php file eg. `sites/default/settings.shared.php`
   and have it include the main database configurations for connecting.

   - If you want to have separate databases for multisite instances the
     database user requires all access so it can create and use newly created
     databases.
   - If you want to use shared tables (eg. users) you should set this up as
     well.

3. Create a multisite default settings.php file eg.
   `sites/default/settings.multisite.php` that includes the shared settings
   file as well as other possible multisite configurations.

   This file will be read and appended with per multisite configurations
   while an instance is created. This is done by adding to the array, eg.
   $databases['default']['default']['prefix']['default'] = 'subdomain.';
   So for shared tables to work the prefixes should be set up correctly.

4. Reuse the shared settings file in your main `settings.php` file as well.

5. Configure variables in your `settings.php`

   ```php
   // Shared users table variable required in multisitecreate_profile.
   $conf['multisitecreate_user_table'] = 'shared.users';
   // Role for the admin user created in a new multisite instance.
   $conf['multisitecreate_admin_role'] = 'blogger';
   // The email for the registered temporary admin user.
   $conf['multisitecreate_temp_email'] = 'admin@example.org';
   ```

5. Enable the module

6. Configure how multisites are created at admin/people/multisitecreate/settings

7. Visit admin/people/multisitecreate and create a new multisite

EXAMPLE SETTINGS CONFIGURATIONS
-------------------------------

This is an example settings file setup for creating multisite instances with
separate databases and shared user tables.

### sites/default/settings.php

```php
<?php

/**
 * @file
 * Configurations for the main site.
 */

include __DIR__ . '/settings.shared.php';

$drupal_hash_salt = '...';
$conf['devel_xhprof_url'] = 'https://localhost:4545/xhprof';

$_whitelist = array(
  'example.org',
  'www.example.org',
  'dev.example.org',
);
// If the domain doesnt have a subdomain directory and isnt whitelisted return a 404.
// This applies to randomly inputing subdomains when using wildcard vhosts.
if (php_sapi_name() != 'cli' && !in_array($_SERVER['HTTP_HOST'], $_whitelist) && conf_path() == 'sites/default') {
  drupal_add_http_header('Status', '404 Not Found');
  $fast_404_html = variable_get('404_fast_html');
  print strtr($fast_404_html, array('@path' => check_plain(request_uri())));
  exit;
}
```

### sites/default/settings.shared.php

```php

# .... the contents of default.settings.php..

// Shared users table variable required in multisitecreate_profile.
$conf['multisitecreate_user_table'] = 'shared.users';
// Role for the admin user created in a new multisite instance.
$conf['multisitecreate_admin_role'] = 'blogger';
// The email for the registered temporary admin user.
$conf['multisitecreate_temp_email'] = 'admin@example.org';

if (file_exists(__DIR__ .'/settings.local.php')) {
  include __DIR__ . '/settings.local.php';
}

```

### sites/default/settings.local.php

```php
<?php

/**
 * @file
 * Host specific configurations, this file is not tracked in git but unique per
 * environment.
 */

$databases['default']['default'] = array(
  'database' => '...',
  'username' => '...',
  'password' => '...',
  'host' => 'localhost',
  'port' => '',
  'driver' => 'mysql',
  'prefix' => array(
    'default' => 'main.',
    'users' => 'shared.',
    'sessions' => 'shared.',
    'authmap' => 'shared.',
    // table is used to memorie the user which initialized the site
    // create process. This table must exist in a database!
    'cache_multisitecreate' => 'shared.',
  ),
);

$cookie_domain = '.example.org';
```

### sites/default/settings.multisite.php

```php
<?php

/**
 * @file
 * Boilerplate for multisite instances, this loads the default settings such as
 * database configurations but then overrides with multisite specific
 * configurations.
 */

include DRUPAL_ROOT . '/sites/default/shared.settings.php';
```

### Example scaffolded settings: /sites/foobar.example.org/settings.php

```php
<?php

/**
 * @file
 * Boilerplate for multisite instances, this loads the default settings such as
 * database configurations but then overrides with multisite specific
 * configurations.
 */

include DRUPAL_ROOT . '/sites/default/shared.settings.php';
// On installation and update runs separate tables are required.
if (preg_match('@^/(install|update)\.php@', request_uri(), $matches) && !isset($install_done)) {
  $databases['default']['default']['prefix'] = 'foobar_example_com.';
}
// Once installed, use the shared tables.
else {
  $databases['default']['default']['prefix']['default'] = 'foobar_example_com.';
}
 
$databases['default']['default']['database'] = 'foobar_example_com';
 
$drupal_hash_salt = '.................';
```
