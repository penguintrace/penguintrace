#!/bin/bash

SEC_PROFILE=unconfined
sudo -l apparmor_parser > /dev/null
if [ "$?" == "0" ]; then
  sudo apparmor_parser -r -W misc/apparmor-profile
  SEC_PROFILE=penguintrace-profile
fi

docker run -it -p 8080 --tmpfs /tmp:exec --cap-add=SYS_PTRACE --cap-add=SYS_ADMIN --rm --security-opt apparmor=$SEC_PROFILE penguintrace penguintrace
