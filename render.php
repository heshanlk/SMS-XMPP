<?php

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
ob_end_flush();
//echo $db_url;
//echo "\n";
// get the command line values
//print_r($argv);
$uid = $argv[1];
set_time_limit(0);
register_shutdown_function('shutdown', $uid);
// infinite loop to get messages for the online users
module_load_include('inc', 'im_plus_plus', 'includes/xmpphp-drupal');
$user = null;
$user = db_fetch_object(db_query("SELECT uid, address, email, data FROM {im_plus_plus_user} WHERE uid = %d", $uid));
watchdog('IM++', 'Starting chat receiver for <em>!address</em>', array('!address' => $user->email), WATCHDOG_DEBUG);
//print_r($user);
$xmpphp = null;
//global $xmpphp;
$xmpphp = new XMPPHP_Drupal($user);
$xmpphp->execute();
$xmpphp->disconnect();
unset($xmpphp);

//if (!$xmpphp->execute())
//    $message = 'Username and pass did not match!. Please retry.';
//else
//    $message = 'You have successfully logged in the Google Talk!.';
//_im_plus_plus_send_outgoing_sms($address, $message);

function shutdown($uid) {
  $user = db_fetch_object(db_query("SELECT timestamp FROM {im_plus_plus_user} WHERE uid = %d", $uid));
  if (($user->timestamp + 300) < time()) {
    db_query("UPDATE {im_plus_plus_user} SET status = %d WHERE uid = %d", AWAY, $uid);
    db_query('DELETE FROM {im_plus_plus_user_addressbook} WHERE uid = %d', $uid);
  }
}