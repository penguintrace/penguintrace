#!/bin/bash

SEC_PROFILE=unconfined
sudo -l apparmor_parser > /dev/null
if [ "$?" == "0" ]; then
  echo "Using [sudo] to install apparmor-profile"
  sudo apparmor_parser -r -W misc/apparmor-profile
  SEC_PROFILE=penguintrace-profile
fi

docker run -it -p 127.0.0.1:8080:8080 --tmpfs /tmp:exec --cap-add=SYS_PTRACE --cap-add=SYS_ADMIN --rm --security-opt apparmor=$SEC_PROFILE penguintrace penguintrace
