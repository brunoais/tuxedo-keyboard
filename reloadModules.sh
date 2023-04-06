#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

systemctl stop tccd $(cat "$DIR"/.extra_restart 2> /dev/null)

sleep 0.5

for module in $(tac ./modules.order) ; do
	rmmod $(echo "$module" | sed -E 's/^.*\/([^/]+?)\.ko$/\1/');
done;
for module in $(cat ./modules.order) ; do
	insmod "$module";
done;

systemctl start tccd $(cat "$DIR"/.extra_restart 2> /dev/null)
