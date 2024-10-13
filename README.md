------------------------------------------------------------------
# instruction for installation / prerequisites:
------------------------------------------------------------------
## GIT
------------------------------------------------------------------
```
sudo apt-get update
sudo apt-get install git
```
------------------------------------------------------------------
## PAHO
------------------------------------------------------------------
```
cd $home
mkdir mqttclients
cd mqttclients
sudo git clone https://github.com/janderholm/paho.mqtt.c.git
apt-cache policy openssl
sudo apt-get install libssl-dev
cd paho.mqtt.c
sudo make		(can receive a lot of warning)
sudo make install	(can receive an error)
```
------------------------------------------------------------------
## ASYNC folder
------------------------------------------------------------------
```
cd $home
mkdir async
```
------------------------------------------------------------------
## EASYSOCKET
------------------------------------------------------------------
```
cd $home
cd async
git clone https://github.com/papergion/easysocket.git
cd easysocket
mv MakeHelper ..
```
------------------------------------------------------------------
## SCSGATE
------------------------------------------------------------------
```
cd $home
cd async
git clone https://github.com/papergion/raspy_scsgate_cpp.git
cd raspy_scsgate_cpp
make
```
   executable in bin/release directory
