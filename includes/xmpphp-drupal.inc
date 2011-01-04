<?php

#Use XMPPHP_Log::LEVEL_VERBOSE to get more logging for error reports
#If this doesn't work, are you running 64-bit PHP with < 5.2.6?
//include 'library/XMPPHP/XMPP.php';
module_load_include('php', 'im_plus_plus', 'library/XMPPHP/XMPP');

class XMPPHP_Drupal {

  protected $conn;
  protected $user;

  public function __construct($user, $server = 'talk.google.com', $port = 5222, $host = 'gmail.com') {
    $service = db_fetch_object(db_query("SELECT service FROM {im_plus_plus_user} WHERE uid = %d", $user->uid));
    $im = $service->service;
    if (class_exists($im)) {
      $service_instance = new $im;
      $server_info = $service_instance->get_info();
      $server = $server_info['server'];
      $port = $server_info['port'];
      $host = $server_info['host'];
      watchdog('IM++', 'Using class !service', array('!service' => $im), WATCHDOG_DEBUG);
    } else {
      _im_plus_plus_send_outgoing_sms($user, 'wrong_format_help');
      watchdog('IM++', 'Class does not exits !service', array('!service' => $im), WATCHDOG_ERROR);
      return;
    }
    $conn = null;
    $conn = new XMPPHP_XMPP($server, $port, $user->email, base64_decode($user->data), 'xmpphp', $host, $printlog = FALSE, $loglevel = XMPPHP_Log::LEVEL_DEBUG);
    $conn->autoSubscribe();
    $conn->connect();
    $this->conn = $conn;
    $this->user = $user;
    watchdog('IM++', 'Start connection to !server:!port using !username@!host', array('!server' => $server, '!port' => $port, '!username' => $user->email, '!host' => $host, '!pass' => base64_decode($user->data)), WATCHDOG_DEBUG);
  }

  public function __destruct() {
    unset($this->conn);
    unset($this->user);
  }

  /**
   * Check for disconected
   * @return <type>
   */
  public function isDisconnected() {
    $conn = $this->conn;
    if ($conn->isDisconnected())
      return TRUE;
    else
      return FALSE;
  }

  /**
   * re connect
   */
  public function reConnect() {
    $conn = $this->conn;
    $conn->doReconnect();
  }

  /**
   * Execute optinos
   * @param <type> $options
   * @return <type> 
   */
  public function execute($options = array('message', 'presence', 'end_stream', 'session_start', 'vcard')) {
    $conn = $this->conn;
    $user = $this->user;
    $vcard_request = array();
    try {
      $start = time();
      while (!$conn->isDisconnected()) {
        $payloads = $conn->processUntil($options);
        foreach ($payloads as $event) {
          $pl = $event[1];
          switch ($event[0]) {
            case 'message':
              $from = $pl['from'];
              $message = $pl['body'];
              if (strlen($message) > 0) {
                watchdog('IM++', 'Receive chat message <em>!message</em> from <em>!from</em> to <em>!to</em>', array('!message' => $message, '!from' => $from, '!to' => $user->email), WATCHDOG_DEBUG);
//                                print_r($user);
                $from = explode('/', $from);
                $from = db_fetch_object(db_query("SELECT id, name FROM {im_plus_plus_user_addressbook} WHERE email = '%s'", $from));
                $from_name = explode(' ', $from->name);
                _im_plus_plus_send_outgoing_sms($user, 'search', array('body' => "($from->id) " . $from_name[0] . ' says, ' . $message));
              }
              // send SMS
//              print "---------------------------------------------------------------------------------\n";
//              print "Message from: {$pl['from']}\n";
//                            if ($pl['subject'])
//                                print "Subject: {$pl['subject']}\n";
//              print $pl['body'] . "\n";
//                            print "---------------------------------------------------------------------------------\n";
//                            $conn->message($pl['from'], $body = "Thanks for sending me \"{$pl['body']}\".", $type = $pl['type']);
              $cmd = explode(' ', $pl['body']);
              if ($cmd[0] == 'quit')
                $conn->disconnect();
              if ($cmd[0] == 'break')
                $conn->send("</end>");
//              if ($cmd[0] == 'vcard') {
//                if (!($cmd[1]))
//                  $cmd[1] = $conn->user . '@' . $conn->server;
//                // take a note which user requested which vcard
//                $vcard_request[$pl['from']] = $cmd[1];
//                // request the vcard
//                $conn->getVCard($cmd[1]);
//              }
              break;
            case 'presence':
              // add buddies to the database, remove if already exitsts
              watchdog('IM++', '!user is <em>!status</em>', array('!user' => $pl['from'], '!status' => $pl['show']), WATCHDOG_DEBUG);
              $mail = explode('/', $pl['from']);
              if ($pl['show'] == 'unavailable') {
                db_query('DELETE FROM {im_plus_plus_user_addressbook} WHERE email = \'%s\'', $mail[0]);
                // delete the user from table
              } else {
                $conn->getVCard($pl['from']);
                $buddy = db_fetch_object(db_query("SELECT uid FROM {im_plus_plus_user_addressbook} WHERE uid = %d and email = '%s'", $user->uid, $mail[0]));
                if (empty($buddy))
                  db_query("INSERT INTO {im_plus_plus_user_addressbook} (uid, name, email, timestamp) VALUES (%d, '%s', '%s', '%s')", $user->uid, $mail[0], $mail[0], time());
                // add new user to the table
              }
//              print "Presence: {$pl['from']} [{$pl['show']}] {$pl['status']}\n";
              break;
            case 'session_start':
//                            print "Session Start\n";
              $conn->getRoster();
              $conn->presence("is on Etisalat SMS IM++");
//                            $conn->presence();
              watchdog('IM++', 'Session start for !user', array('!user' => $user->email), WATCHDOG_DEBUG);
              break;
            case 'vcard':
              $name = $pl['fn'];
              $mail = explode('/', $pl['from']);
              db_query("UPDATE {im_plus_plus_user_addressbook} SET name = '%s' WHERE email = '%s'", $name, $mail[0]);
//              watchdog('IM++', '!user name chaned to <em>!name</em>', array('!user' => $mail[0], '!name' => $name), WATCHDOG_DEBUG);
              break;
          }
        }
        // disonnect the connection once it timeout, set time out to 5min
        $time_diff = time() - $start;
        if ($time_diff > 300) {
          echo 'Terminating ' . $time_diff;
          $conn->disconnect();
        }
      }
    } catch (XMPPHP_Exception $e) {
//            die($e->getMessage());
      return FALSE;
    }
  }

  /**
   * Get messages
   * @param <type> $options
   * @return <type> 
   */
  public function messages($options = array('message')) {
    $conn = $this->conn;
    $user = $this->user;
    $vcard_request = array();
    try {
      while (!$conn->isDisconnected()) {
        $payloads = $conn->processUntil($options);
        foreach ($payloads as $event) {
          $pl = $event[1];
          switch ($event[0]) {
            case 'message':
              $from = $pl['from'];
              $message = $pl['body'];
              if (strlen($message) > 0) {
                watchdog('IM++', 'Receive chat message !message from <em>!from</em> to <em>!to</em>', array('!message' => $message, '!from' => $from, '!to' => $user->email), WATCHDOG_DEBUG);
                $from = explode('/', $from);
//              _im_plus_plus_send_outgoing_sms($user->address, $from[0] . ' says, ' . $message);
              }
              break;
          }
        }
      }
    } catch (XMPPHP_Exception $e) {
      return FALSE;
    }
  }

  /**
   * Presences
   * @param <type> $options
   * @return <type>
   */
  public function presence($options = array('presence')) {
    $conn = $this->conn;
    $user = $this->user;
    $vcard_request = array();
    try {
      while (!$conn->isDisconnected()) {
//                $payloads = $conn->processUntil(array('message', 'presence', 'end_stream', 'session_start', 'vcard'));
        $payloads = $conn->processUntil($options);
        foreach ($payloads as $event) {
          $pl = $event[1];
          switch ($event[0]) {
            case 'presence':
              // add buddies to the database, remove if already exitsts
              watchdog('IM++', '!user is <em>!status</em>', array('!user' => $pl['from'], '!status' => $pl['show']), WATCHDOG_DEBUG);
              $mail = explode('/', $pl['from']);
              if ($pl['show'] == 'unavailable') {
                db_query('DELETE FROM {im_plus_plus_user_addressbook} WHERE email = \'%s\'', $mail[0]);
                // delete the user from table
              } else {
                $buddy = db_fetch_object(db_query("SELECT uid FROM {im_plus_plus_user_addressbook} WHERE uid = %d and email = '%s'", $user->uid, $mail[0]));
                if (empty($buddy))
                  db_query("INSERT INTO {im_plus_plus_user_addressbook} (uid, name, email, timestamp) VALUES (%s, '%s', '%s', '%s')", $user->uid, $mail[0], $mail[0], time());
                // add new user to the table
              }
              break;
          }
        }
      }
    } catch (XMPPHP_Exception $e) {
      return FALSE;
    }
  }

  /**
   * Send IM message
   * @param <type> $to
   * @param <type> $message
   */
  public function send_im($to, $message) {
    $conn = $this->conn;
    try {
      $conn->processUntil('session_start');
      $conn->presence("is on Etisalat SMS IM++");
      $conn->message($to, $message);
      $conn->disconnect();
    } catch (XMPPHP_Exception $e) {
      die($e->getMessage());
    }
  }

  /**
   * SEt status
   * @param <type> $status
   */
  public function set_status($status) {
    $conn = $this->conn;
    try {
      $conn->processUntil('session_start');
      $conn->getRoster();
      $conn->presence($status);
      $conn->disconnect();
    } catch (XMPPHP_Exception $e) {
      die($e->getMessage());
    }
  }

  /**
   * Logout from server
   */
  public function logout() {
    $conn = $this->conn;
    try {
      $conn->disconnect();
    } catch (XMPPHP_Exception $e) {
      die($e->getMessage());
    }
  }

  /**
   * Login to IM
   * @return boolean 
   */
  public function login() {
    $auth = FALSE;
    $conn = $this->conn;
    $user = $this->user;

    $payloads = $conn->processUntil(array('session_start'));
    foreach ($payloads as $event) {
      $pl = $event[1];
      switch ($event[0]) {
        case 'session_start':
          $conn->getRoster();
          $conn->presence("is on Etisalat SMS IM++");
          watchdog('IM++', 'Session start for !user', array('!user' => $user->email), WATCHDOG_INFO);
          $auth = TRUE;
//              break;
      }
    }
//    $this->messages();
    $conn->disconnect();
    return $auth;
  }

  /**
   * disconnect form the server.
   */
  public function disconnect() {
    $conn = $this->conn;
    try {
      $conn->disconnect();
    } catch (XMPPHP_Exception $e) {
      die($e->getMessage());
    }
  }

}

/**
 *  Inteface for im agents
 */
interface im_agent {
  public function get_info();
}

/**
 * implementing Facebook agent
 */
class fb implements im_agent {

  protected $conf = array();

  public function __construct() {
    $conf = array(
      'host' => 'chat.facebook.com',
      'server' => 'chat.facebook.com',
      'port' => 5222
    );
    $this->conf = $conf;
  }

  public function get_info() {
    return $this->conf;
  }

}

/**
 * implementing gTalk agent
 */
class gt implements im_agent {

  protected $conf = array();

  public function __construct() {
    $conf = array(
      'host' => 'gmail.com',
      'server' => 'talk.google.com',
      'port' => 5222
    );
    $this->conf = $conf;
  }

  public function get_info() {
    return $this->conf;
  }

}