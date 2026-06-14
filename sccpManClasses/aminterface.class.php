<?php

/**
 *
 * Core Comsnd Interface
 *
 *  https://www.voip-info.org/asterisk-manager-example-php/
 */

namespace FreePBX\modules\Sccp_manager;

class aminterface
{
    // Объявляем все свойства как public для совместимости с PHP 8.2
    public $paren_class;
    public $_socket;
    public $_error;
    public $_config;
    public $_test;
    public $_connect_state;
    public $_lastActionClass;
    public $_lastActionId;
    public $_lastRequestedResponseHandler;
    public $_ProcessingMessage;
    public $_DumpMessage;
    public $debug_level = 1;
    public $_incomingRawMessage;
    public $eventListEndEvent;
    public $_context;
    public $_eventListeners;
    public $_incomingMsgObjectList;
    public $eventListIsCompleted;
    public $AmiEventHandlers;
    public $AmiResponseHandlers;

    public function load_subspace($parent_class = null)
    {
        $driverNamespace = "\\FreePBX\\Modules\\Sccp_manager\\aminterface";

        $drivers = array('Message' => 'Message.class.php',
                  'Response' => 'Response.class.php',
                  'Event' => 'Event.class.php'
                );
        foreach ($drivers as $key => $value) {
            $class = $driverNamespace . "\\" . $key;
            $driver = __DIR__ . "/amInterfaceClasses/" . $value;
            if (!class_exists($class, false)) {
                if (file_exists($driver)) {
                    include(__DIR__ . "/amInterfaceClasses/" . $value);
                } else {
                    throw new \Exception("Class required but file not found " . $driver);
                }
            }
        }
    }

    public function __construct($parent_class = null)
    {
        global $amp_conf;
        $this->paren_class = $parent_class;
        $this->_socket = false;
        $this->_connect_state = false;
        $this->_error = array();
        $this->_config = array('host' => 'localhost',
                        'user' => '',
                        'pass' => '',
                        'port' => '5038',
                        'tsoket' => 'tcp://',
                        'timeout' => 30,
                        'enabled' => true
                      );
        $this->_eventListeners = array();
        $this->_incomingMsgObjectList = array();
        $this->_lastActionId = false;
        $this->_incomingRawMessage = array();
        $this->eventListEndEvent = '';

        $fld_conf = array('user' => 'AMPMGRUSER', 'pass' => 'AMPMGRPASS');
        if (isset($amp_conf['AMPMGRUSER'])) {
            foreach ($fld_conf as $key => $value) {
                if (isset($amp_conf[$value])) {
                    $this->_config[$key] = $amp_conf[$value];
                }
            }
        }
        if ($this->_config['enabled']) {
            $this->load_subspace();
        }

        if ($this->_config['enabled']) {
            // Ami is not hard disabled in __construct line 63.
            if ($this->open()) {
                // Can open a connection. Now check compatibility with chan-sccp.
                // will return true if compatible.
                if (!$this->get_compatible_sccp(true)[1]) {
                    // Close the open socket as will not use
                    $this->close();
                }
            }
        }
    }

    public function info()
    {
        $Ver = '13.0.4';
        if ($this->_config['enabled']){
            return array('Version' => $Ver,
                'about' => 'AMI data ver: ' . $Ver, 'test' => get_declared_classes());
        } else {
            return array('Version' => $Ver,
                'about' => 'Disabled AMI  ver: ' . $Ver);
        }
    }

    /*
     * Opens a socket connection to ami.
     */
    public function open()
    {
        $cString = $this->_config['tsoket'] . $this->_config['host'] . ':' . $this->_config['port'];
        $this->_context = stream_context_create();
        $errno = 0;
        $errstr = '';
        $this->_ProcessingMessage = '';
        $this->_socket = @stream_socket_client(
            $cString,
            $errno,
            $errstr,
            $this->_config['timeout'],
            STREAM_CLIENT_CONNECT,
            $this->_context
        );
        if ($this->_socket === false) {
            $this->_errorException('Error connecting to ami: ' . $errstr . $cString);
            return false;
        }
        $msg = new aminterface\LoginAction($this->_config['user'], $this->_config['pass']);
        $response = $this->send($msg);

        if ($response != false) {
            if (!$response->isSuccess()) {
                $this->_errorException('Could not connect: ' . $response->getMessage());
                return false;
            } else {
                @stream_set_blocking($this->_socket, 0);
                $this->_connect_state = true;
                $this->_ProcessingMessage = '';
            }
        }
        return true;
    }

    /*
     * Closes the connection to ami.
     */
    public function close()
    {
        $this->_connect_state = false;
        $this->_ProcessingMessage = '';
        @stream_socket_shutdown($this->_socket, STREAM_SHUT_RDWR);
    }
    /*
    * Send action message to ami, and wait for Response
    */
public function send($message)
    {
        $_incomingRawMessage = array();
        $messageToSend = $message->serialize();
        $length = strlen($messageToSend);
        $this->_DumpMessage = '';
        $this->_lastActionId = $message->getActionID();
        $this->_lastRequestedResponseHandler = $message->getResponseHandler();
        $this->_lastActionClass = $message;
        $this->_incomingRawMessage[$this->_lastActionId] = '';
        $this->eventListIsCompleted = array();
        if (@fwrite($this->_socket, $messageToSend) < $length) {
            $this->_errorException('Could not send message');
            return false;
        }
        // Have sent a message and now have to wait for and read the reply
        // The below infinite loop waits for $this->completed to be true.
        // The loop calls readBuffer, which calls GetMessages, which calls Process
        // This loop then continues until we have _thisComplete as an object variable
        $this->eventListIsCompleted[$this->_lastActionId] = false;
        while (true) {
            stream_set_timeout($this->_socket, 1);
            $this->readBuffer();
            $info = stream_get_meta_data($this->_socket);
            if ($info['timed_out'] == true) {
                $this->_errorException("Read waittime: " . ($this->socket_param['timeout']) . " exceeded (timeout).\n");
                return false;
            }
            if ($this->eventListIsCompleted[$this->_lastActionId]) {
                $response = $this->_incomingMsgObjectList[$this->_lastActionId];
                // need to test that the list was successfully completed here
                $allReceived = $response->getClosingEvent()
                                ->listCorrectlyReceived($this->_incomingRawMessage[$this->_lastActionId],
                                $response->getCountOfEvents());
                // now tidy up removing any temp variables or objects
                $response->removeClosingEvent();
                unset($_incomingRawMessage[$this->_lastActionId]);
                unset($this->_incomingMsgObjectList[$this->_lastActionId]);
                unset($this->_lastActionId);

                if (!$allReceived) {
                    // Something is missing from the events list received via AMI, or
                    // the control parameter at the end of the list has changed.
                    // We simply echo the warning instead of throwing a fatal exception
                    // and return the response anyway.
                    echo substr(strrchr(get_class($response), '\\'), 1), " Counts do not match on returned AMI Result\n";
                }
                return $response;
            }
        }
    }

    protected function readBuffer ()
    {
        $read = @fread($this->_socket, 65535);
        // AMI never returns EOF
        if ($read === false ) {
            $this->_errorException('Error reading');
        }
        // Do not return empty Messages
        while ($read == "" ) {
            $read = @fread($this->_socket, 65535);
        }
        // Add read to the rest of buffer from previous read
        $this->_ProcessingMessage .= $read;
        $this->getMessages();
    }

    protected function getMessages()
    {
        $msgs = array();
        // Extract any complete messages and leave remainder for next read
        while (($marker = strpos($this->_ProcessingMessage, aminterface\Message::EOM))) {
            $msg = substr($this->_ProcessingMessage, 0, $marker);
            $this->_ProcessingMessage = substr(
                $this->_ProcessingMessage,
                $marker + strlen(aminterface\Message::EOM)
            );
            $msgs[] = $msg;
        }
        $this->process($msgs);
    }

    public function process(array $msgs)
    {
        foreach ($msgs as $aMsg) {
            // 2 types of message; Response or Event. Response only incudes data
            // for JSON response and Command response. All other responses expect
            // data in an event list - these events need to be attached to the response.
            $resPos = strpos($aMsg, 'Response: ');   // Have a Response message. This may not be 0.
            $evePos = strpos($aMsg, 'Event: ');   // Have an Event Message. This should always be 0.
            // Add the incoming message to a string that can be checked
            // against the completed message event when it is received.
            $this->_incomingRawMessage[$this->_lastActionId] .= "\r\n\r\n" . $aMsg;
            if (($resPos !== false) && (($resPos < $evePos) || $evePos === false)) {
                $response = $this->_responseObjFromMsg($aMsg); // resp Ok
                $this->eventListEndEvent = $response->getKey('eventlistendevent');
                $this->_incomingMsgObjectList[$this->_lastActionId] = $response;
                $this->eventListIsCompleted[$this->_lastActionId] = $response->isComplete();
            } elseif ($evePos === 0) {      // Event must be at the start of the msg.
                $event = $this->_eventObjFromMsg($aMsg); // Event  Ok
                $this->eventListIsCompleted[$this->_lastActionId] = $event->isComplete();
                $this->_incomingMsgObjectList[$this->_lastActionId]->addEvent($event);
            } else {
                // broken ami most probably through changes in chan_sccp_b.
                // AMI is sending a message which is neither a response nor an event.
                $this->_msgToDebug(1, 'resp broken ami');
                $bMsg = 'Event: ResponseEvent' . "\r\n";
                $bMsg .= 'ActionId: ' . $this->_lastActionId . "\r\n" . $aMsg;
                $event = $this->_eventObjFromMsg($bMsg);
                $this->_incomingMsgObjectList[$this->_lastActionId]->addEvent($event);
            }
        }
    }

    private function _msgToDebug($level, $msg)
    {
        if ($level > $this->debug_level) {
            return;
        }
        print_r('<br> level: '.$level.' ');
        print_r($msg);
        print_r('<br>');
    }

    private function _responseObjFromMsg($message)
    {
        $_className = false;

        $responseClass = '\\FreePBX\\modules\\Sccp_manager\\aminterface\\Generic_Response';
        if ($this->_lastRequestedResponseHandler != false) {
            $_className = '\\FreePBX\\modules\\Sccp_manager\\aminterface\\' . $this->_lastRequestedResponseHandler . '_Response';
        }
        if ($_className) {
            if (class_exists($_className, true)) {
                $responseClass = $_className;
            } elseif ($responseHandler != false) {
                $this->_errorException('Response Class ' . $_className . '  requested via responseHandler, could not be found');
            }
        }
        $response = new $responseClass($message);
        $actionId = $response->getActionID();
        if ($actionId === null) {
            $response->setActionId($this->_lastActionId);
        }
        return $response;
    }
    public function _eventObjFromMsg($message)
    {
        $eventType = explode(aminterface\Message::EOL,$message,2);
        $name = trim(explode(':',$eventType[0],2)[1]);
        $className = '\\FreePBX\\modules\\Sccp_manager\\aminterface\\' . $name . '_Event';
        if (class_exists($className, true) === false) {
            $className = '\\FreePBX\\modules\\Sccp_manager\\aminterface\\UnknownEvent';
        }
        return new $className($message);
    }

    protected function dispatch($message)
    {
        print_r("<br>------------dispatch----------<br>");
        print_r($message);
        return false;
        die();
        foreach ($this->_eventListeners as $data) {
            $listener = $data[0];
            $predicate = $data[1];
            print_r($data);

            if (is_callable($predicate) && !call_user_func($predicate, $message)) {
                continue;
            }
            if ($listener instanceof \Closure) {
                $listener($message);
            } elseif (is_array($listener)) {
                $listener[0]->$listener[1]($message);
            } else {
                $listener->handle($message);
            }
        }
        print_r("<br>------------E dispatch----------<br>");
    }
//-------------------Adaptive Function ------------------------------------------------------------

    function core_list_hints()
    {
        $result = array();
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\ExtensionStateListAction();
            $_response = $this->send($_action);
            $_res = $_response->getResult();
            foreach ($_res as $key => $value) {
                foreach ($value as $key2 => $value2) {
                    $result[$key2] = '@' . $key2;
                }
            }
        }
        return $result;
    }

    function core_list_all_hints()
    {
        $result = array();
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\ExtensionStateListAction();
            $_res = $this->send($_action)->getResult();
            foreach ($_res as $key => $value) {
                foreach ($value as $key2 => $value2) {
                    $result[$key.'@'.$key2] = $key.'@'.$key2;
                }
            }
        }
        return $result;
    }
// --------------------- SCCP Comands
    function sccp_list_keysets()
    {
        $result = array();
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\SCCPShowSoftkeySetsAction();
            $_res = $this->send($_action)->getResult();
            foreach ($_res as $key => $value) {
                $result[$key] = $key;
            }
        }
        return $result;
    }
    function sccp_get_active_device()
    {
        $result = array();
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\SCCPShowDevicesAction();
            $result = (array)$this->send($_action)->getResult();
        }
        return $result;
    }
    function sccp_getdevice_info($devicename)
    {
        $result = array();
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\SCCPShowDeviceAction($devicename);
            $result = $this->send($_action)->getResult();
            $result['MAC_Address'] = $result['macaddress'];
        }
        return $result;
    }
    function sccpDeviceReset($devicename, $action = '')
    {
        if ($this->_connect_state) {
            if ($action == 'tokenack') {
                $_action = new \FreePBX\modules\Sccp_manager\aminterface\SCCPTokenAckAction($devicename);
            } else {
                $_action = new \FreePBX\modules\Sccp_manager\aminterface\SCCPDeviceRestartAction($devicename, $action);
            }
            $_response = $this->send($_action);
            $result['data'] = 'Device: '.$devicename.' Result: '.$_response->getMessage();
            $result['Response']=$_response->getKey('Response');
        }
        return $result;
    }

//------------------- Core Comands ----
    function core_sccp_reload()
    {
        $result = array();
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\ReloadAction('chan_sccp');
            $result = ['Response' => $this->send($_action)->getMessage(), 'data' => ''];
        }
        return $result;
    }
    function getSCCPConfigMetaData($segment = '') {
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\SCCPConfigMetaDataAction($segment);
            $metadata = $this->send($_action)->getResult();
        }
        return $metadata;
    }

    function getSCCPVersion()
    {
        //Initialise result array
        $result = array( 'RevisionHash' => '', 'vCode' => 0, 'RevisionNum' => 0, 'buildInfo' => '', 'Version' => 0);
        $metadata = $this->getSCCPConfigMetaData();
        if (isset($metadata['Version'])) {
            $result['Version'] = $metadata['Version'];
            $version_parts = array_map('intval',explode('.', $metadata['Version']));
            if ($version_parts[0] === 4) {
                switch ($version_parts[1]) {
                    case 1:
                        $result['vCode'] = 410;
                        break;
                    case 2:
                        $result['vCode'] = 420;
                        break;
                    case 3:
                        $result['vCode'] = 430;
                        if($version_parts[2] >= 3){
                            $result['vCode'] = 433;
                        }
                        break;
                    default:
                        $result['vCode'] = 400;
                        break;
                }
            }
            if (array_key_exists("RevisionHash", $metadata)) {
                $result['RevisionHash'] = $metadata["RevisionHash"];
            }
            if (isset($metadata['RevisionNum'])) {
                if ($metadata['RevisionNum'] >= 11063) { // new method, RevisionNum is incremental
                    $result['vCode'] = 433;
                }
                $result['RevisionNum'] = $metadata["RevisionNum"];
            }
            if (isset($metadata['ConfigureEnabled'])) {
                $result['buildInfo'] = $metadata['ConfigureEnabled'];
            }
        }
        return $result;
    }

    function getRealTimeStatus()
    {
        // Initialise array with default values to eliminate testing later
        $result = array();
        $cmd_res = array();
        $cmd_res = ['sccp' => ['message' => 'legacy value', 'realm' => '', 'status' => 'ERROR']];
        if ($this->_connect_state) {
            $_action = new \FreePBX\modules\Sccp_manager\aminterface\CommandAction('realtime mysql status');
            $result = $this->send($_action)->getResult();
         }
         if (is_array($result['Output'])) {
             foreach ($result['Output'] as $aline) {
                 if (strlen($aline) > 3) {
                     $temp_strings = explode(' ', $aline);
                     $cmd_res_key = $temp_strings[0];
                     foreach ($temp_strings as $test_string) {
                          if (strpos($test_string, '@')) {
                            $this_realm = $test_string;
                            break;
                          }
                     }
                     $cmd_res[$cmd_res_key] = array('message' => $aline, 'realm' => $this_realm, 'status' => strpos($aline, 'connected') ? 'OK' : 'ERROR');
                 }
            }
        }
        return $cmd_res;
    }

    public function get_compatible_sccp($revNumComp=false) {
        // only called with args from installer to get revision and compatibility
        $res = $this->getSCCPVersion();
        if ($res['RevisionNum'] < 11063) {
            $this->useAmiInterface = false;
        }
        if ($revNumComp) {
            return array($res['vCode'], true);
        }
        return $res['vCode'];
    }
}
