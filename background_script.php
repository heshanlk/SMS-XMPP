<?php

//$stdout = fopen('php://stdout', 'w');
//fwrite($stdout, "Script Template\n");
//print_r($_SERVER);
// activate full error reporting
error_reporting(E_ALL);

$drupal_base_url = parse_url('http://app.heidisoft.com/');
$_SERVER['HTTP_HOST'] = $drupal_base_url['host'];
$_SERVER['PHP_SELF'] = $drupal_base_url['path'] . '/index.php';
$_SERVER['REQUEST_URI'] = $_SERVER['SCRIPT_NAME'] = $_SERVER['PHP_SELF'];
$_SERVER['REMOTE_ADDR'] = NULL;
$_SERVER['REQUEST_METHOD'] = NULL;

define('ONLINE', 1);
define('OFFLINE', 5);
define('AWAY', 10);
define('QUEUE', 20);

define('DRUPAL_ROOT', realpath(getcwd()));
include_once DRUPAL_ROOT . '/includes/bootstrap.inc';
// disable error reporting for bootstrap process
error_reporting(E_ERROR);
// let's bootstrap: we will be able to use drupal apis
drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);
// enable full error reporting again
error_reporting(E_ALL);
// turn off the output buffering that drupal is doing by default.
//ob_end_flush();
// get the command line values
//print_r($argv);
//$uid = $argv[1];
// infinite loop to get messages for the online users
set_time_limit(0);
while (TRUE) {
  $results = db_query("SELECT uid FROM {im_plus_plus_user} WHERE status = %d", QUEUE);
  $count = db_result(db_query("SELECT COUNT(uid) FROM {im_plus_plus_user} WHERE status = %d", QUEUE));
  if ($count > 0) {
    while ($user = db_fetch_object($results)) {
//        print_r($user);
      $time = time();
      db_query("UPDATE {im_plus_plus_user} SET timestamp = '%s', status = %d WHERE uid = %d", $time, ONLINE, $user->uid);
      $uid = $user->uid;
      exec("php render.php $uid > /dev/null &");
    }
  } else {
//    echo "sleeping 2sec... \n";
    sleep(1);
  }
}