
# WOL Bot
Is a simple telegram bot for waking up your computer on lan.
All necessary configurations are in defs.h (wifi ssid, passwd, bot token, ...)

## Usage
#### Common commands
##### /wakeup
Wake up computer

##### /reboot
Reboot Controller

##### /reset
Reset configuration to default values (from defs.h)

#### Configuration
##### Key syntax
index ::= [0-9]+
name ::= [a-zA-Z0-9_\\-]+
key ::= <name> ( '[' <index> ']' )?

##### /get <key>
Get config value by key
(supported edit message)

##### /set <key>=<value>
Set config value by key
(supported edit message)

##### /list
Get available config keys

