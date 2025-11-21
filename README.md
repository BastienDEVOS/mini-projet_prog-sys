Pour le systemd il faut faire :  
sudo cp surveillance-daemon.service /etc/systemd/system/  
sudo systemctl daemon-reload  
sudo systemctl enable surveillance-daemon.service  
sudo systemctl start surveillance-daemon.service  
