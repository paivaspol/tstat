#!/bin/bash
rsync -avz $2 -e "ssh -i /Users/vaspol/.ssh/vaspol_aws_key.pem" ubuntu@$1:~/tstat
