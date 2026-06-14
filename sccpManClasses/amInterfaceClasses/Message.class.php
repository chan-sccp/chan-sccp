<?php

/**
 *
 * Core Comsnd Interface
 *
 *  https://www.voip-info.org/asterisk-manager-example-php/
 */
/* !TODO!: Re-Indent this file.  -TODO-: What do you mean? coreaccessinterface  ??  */

namespace FreePBX\modules\Sccp_manager\aminterface;

class AMIException extends \Exception
{

}

abstract class Message
{

    const EOL = "\r\n";
    const EOM = "\r\n\r\n";

    protected $lines;
    protected $variables;
    protected $keys;
    protected $createdDate;
    private $_responseHandler;

    public function __construct()
    {
        $this->lines = array();
        $this->variables = array();
        $this->keys = array();
        $this->createdDate = time();
    }

    public function _ToDebug($level, $msg)
    {
    }

    public function getResponseHandler()
    {
        if (strlen($this->_responseHandler) > 0) {
//            throw new AMIException('Hier:' . $this->_responseHandler);
            return (string) $this->_responseHandler;
        } else {
            return "";
        }
    }

    public function setResponseHandler($responseHandler)
    {
        if (0 == strlen($responseHandler)) {
            return;
        }
        $className = '\\FreePBX\\modules\\Sccp_manager\\aminterface\\' . $responseHandler . '_Response';
        if (class_exists($className, true)) {
            $this->_responseHandler = $responseHandler;
        } else {
            return;
        }
    }

    public function setVariable($key, $value)
    {
        $key = strtolower($key);
        $this->variables[$key] = $value;
        /*        print_r('<br>----Set Value -------<br>');
          print_r($key);
          print_r($value);
         */
    }

    public function getVariable($key)
    {
        $key = strtolower($key);

        if (!isset($this->variables[$key])) {
            return null;
        }
        return $this->variables[$key];
    }

    protected function setKey($key, $value)
    {
        $key = strtolower((string) $key);
        $this->keys[$key] = (string) $value;
        /*
          print_r('<br>----Set Key -------<br>');
          print_r($key);
          print_r($value);
         *
         */
    }

    public function getKey($key)
    {
        $key = strtolower($key);
        if (!isset($this->keys[$key])) {
            return null;
        }
        //return (string)$this->keys[$key];
        return $this->keys[$key];
    }

    public function getVariables()
    {
        return $this->variables;
    }

    public function getActionID()
    {
        return $this->getKey('ActionID');
    }

    public function getKeys()
    {
        return $this->keys;
    }

    private function serializeVariable($key, $value)
    {
        return "Variable: $key=$value";
    }

    protected function setSanitizedKey($key, $value)
    {
        $key = strtolower((string) $key);
        $_string_key = array('actionid', 'descr');
        if (array_search($key, $_string_key) !== false) {
            $this->keys[$key] = (string) $this->sanitizeInput($value, 'string');
        } else {
            $this->keys[$key] = $this->sanitizeInput($value);
        }
    }

    protected function sanitizeInput($value, $prefered_type = '')
    {
        if ($prefered_type == '') {
            if (!isset($value) || $value === null || strlen($value) == 0) {
                return null;
            } elseif (is_numeric($value)) {
                $prefered_type = 'numeric';
            } elseif (is_string($value)) {
                $prefered_type = 'string';
            } else {
                throw new AMIException("Don't know how to convert: '" . $value . "'\n");
            }
        }
        if ($prefered_type !== '') {
            switch ($prefered_type) {
                case 'string':
                    if (!isset($value) || $value === null || strlen($value) == 0) {
                        return '';
                    }
                    if (filter_var($value, FILTER_VALIDATE_BOOLEAN, FILTER_NULL_ON_FAILURE)) {
                        return (boolean) $value;
                    } elseif (filter_var($value, FILTER_DEFAULT, FILTER_NULL_ON_FAILURE)) {
                        return (string) $value;
                    } elseif (filter_var($value, FILTER_SANITIZE_FULL_SPECIAL_CHARS, FILTER_NULL_ON_FAILURE)) {
                        return (string) htmlspecialchars($value, ENT_QUOTES);
                    } else {
                        throw new AMIException("Incoming String is not sanitary. Skipping: '" . $value . "'\n");
                    }
                    break;
                case 'numeric':
                    if (!isset($value) || $value === null || strlen($value) == 0) {
                        return 0;
                    }
                    if (filter_var($value, FILTER_VALIDATE_INT, FILTER_FLAG_ALLOW_HEX | FILTER_FLAG_ALLOW_OCTAL)) {
                        return intval($value, 0);
                    } elseif (filter_var($value, FILTER_VALIDATE_FLOAT, FILTER_FLAG_ALLOW_FRACTION | FILTER_FLAG_ALLOW_THOUSAND | FILTER_FLAG_ALLOW_SCIENTIFIC)) {
                        return (float) $value;
                    } else {
                        return (double) $value;
                    }
                default:
                    throw new AMIException("Don't know how to convert: '" . $value . "'\n");
                    break;
            }
        }
    }

    protected function finishMessage($message)
    {
        return $message . self::EOL . self::EOL;
    }

    public function serialize()
    {
        $result = array();
        foreach ($this->getKeys() as $k => $v) {
            $result[] = $k . ': ' . $v;
        }
        foreach ($this->getVariables() as $k => $v) {
            if (is_array($v)) {
                foreach ($v as $singleValue) {
                    $result[] = $this->serializeVariable($k, $singleValue);
                }
            } else {
                $result[] = $this->serializeVariable($k, $v);
            }
        }
        $mStr = $this->finishMessage(implode(self::EOL, $result));
        return $mStr;
    }

    public function setActionID($actionID)
    {
        if (0 == strlen($actionID)) {
            throw new AMIException('ActionID cannot be empty.');
            return;
        }

        if (strlen($actionID) > 69) {
            throw new AMIException('ActionID can be at most 69 characters long.');
            return;
        }

        $this->setKey('ActionID', $actionID);
    }

    public function __sleep()
    {
        return array('lines', 'variables', 'keys', 'createdDate');
    }
}

abstract class IncomingMessage extends Message
{

    protected $rawContent;

    public function getEventList()
    {
        return $this->getKey('EventList');
    }

    public function getRawContent()
    {
        return $this->rawContent;
    }

    public function isComplete()
    {
        return $this->_completed;
    }

    public function __sleep()
    {
        $ret = parent::__sleep();
        $ret[] = 'rawContent';
        return $ret;
    }

    public function __construct($rawContent)
    {
        parent::__construct();
        $this->rawContent = $rawContent;
        $lines = explode(Message::EOL, $rawContent);
        foreach ($lines as $line) {
            $content = explode(':', $line);
            $name = strtolower(trim($content[0]));
            unset($content[0]);
            $value = isset($content[1]) ? trim(implode(':', $content)) : '';
            try {
                $this->setSanitizedKey($name, $value);
            } catch (AMIException $e) {
                throw new AMIException("Error: '" . $e . "'\n Dump RawContent:\n" . $this->rawContent . "\n");
            }
        }
    }
}

// namespace FreePBX\modules\Sccp_manager\aminterface\Message;
class LoginAction extends ActionMessage
{

    /**
     * Constructor.
     *
     * @param string $user     AMI username.
     * @param string $password AMI password.
     *
     * @return void
     */
    public function __construct($user, $password)
    {
        parent::__construct('Login');
        $this->setKey('Username', $user);
        $this->setKey('Secret', $password);
        $this->setKey('Events', 'off'); // &----
        $this->setResponseHandler('Login');
    }
}

abstract class ActionMessage extends Message
{

    public function __construct($what)
    {
        parent::__construct();
        $this->setKey('Action', $what);
        $this->setKey('ActionID', microtime(true));
    }
}

class CommandAction extends ActionMessage
{
    public function __construct($command)
    {
        parent::__construct('Command');
        $this->setKey('Command', $command);
        $this->setResponseHandler("Command");
    }
}

class ReloadAction extends ActionMessage
{

    public function __construct($module = false)
    {
        parent::__construct('Reload');
        if ($module !== false) {
            $this->setKey('Module', $module);
            $this->setResponseHandler("Generic");
        }
    }
}

class ExtensionStateListAction extends ActionMessage
{

    public function __construct()
    {
        parent::__construct('ExtensionStateList');
        $this->setKey('Segment', 'general');
        $this->setKey('ResultFormat', 'command');
        $this->setResponseHandler("ExtensionStateList");
    }
}
class SCCPShowGlobalsAction extends ActionMessage
{
    public function __construct()
    {
        parent::__construct('SCCPShowGlobals');
    }
}

class SCCPShowSoftkeySetsAction extends ActionMessage
{

    public function __construct()
    {
        parent::__construct('SCCPShowSoftkeySets');
        $this->setKey('Segment', 'general');
        $this->setKey('ResultFormat', 'command');
        $this->setResponseHandler("SCCPShowSoftkeySets");
    }
}

class SCCPShowDeviceAction extends ActionMessage
{

    public function __construct($devicename)
    {
        parent::__construct('SCCPShowDevice');
        $this->setKey('Segment', 'general');
        $this->setKey('ResultFormat', 'command');
        $this->setKey('DeviceName', $devicename);
        $this->setResponseHandler("SCCPShowDevice");
    }
}

class SCCPShowDevicesAction extends ActionMessage
{

    public function __construct()
    {
        parent::__construct('SCCPShowDevices');
        $this->setKey('Segment', 'general');
        $this->setKey('ResultFormat', 'command');
        $this->setResponseHandler("SCCPShowDevices");
    }
}

class SCCPTokenAckAction extends ActionMessage
{

    public function __construct($DeviceName)
    {
        parent::__construct('SCCPTokenAck');
        $this->setKey('DeviceId', $DeviceName);
        $this->setResponseHandler("Generic");
    }
}

class SCCPDeviceRestartAction extends ActionMessage
{

    public function __construct($DeviceName, $Type = "restart")
    {
        parent::__construct('SCCPDeviceRestart');
        $this->setResponseHandler("Generic");
        if (empty($Type)) {
            $Type = "restart";
        }
        $this->setKey('DeviceName', $DeviceName);
        if (in_array(strtolower($Type), array('restart', 'full', 'reset'))) {
            $this->setKey('Type', $Type);
        } else {
            throw new Exception('Param2 has to be one of \'restart\', \'full\', \'reset\'.');
        }
    }
}

class SCCPConfigMetaDataAction extends ActionMessage
{
    public function __construct($segment = false)
    {
        parent::__construct('SCCPConfigMetaData');
        if ($segment != false) {
            $this->setKey('Segment', $segment);
        }
        $this->setResponseHandler("SCCPJSON");
    }
}
