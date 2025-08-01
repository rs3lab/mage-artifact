#!/bin/zsh

# Never call this script directly!
#
# This script is just used to synchronize the "scripts" contained in this
# repo w/ everything else. It doesn't "build" anything, because scripts
# don't build like that.
#
# It's here to support the `manager`'s "send to all `builder` scripts" style
# iface. Most of its commands are dummy.

if [[ -z $MIND_ROOT ]]; then
        echo '$MIND_ROOT not set!' >/dev/stderr
        exit 1
fi
source $MIND_ROOT/scripts/config.sh
cd $MIND_ROOT/mind_linux

function run_command () { 
	echo "$HOST kernel: "
	echo NOT RUNNING "$@"
}

function checkout_kernel_commit () { 
	echo "$HOST: Checking out commit"
	chronic -e git checkout "$@"
}

function show_latest_kernel_commit () { 
	echo "$HOST kernel HEAD commit is:"
	indent git --no-pager log --color=always -3 --abbrev-commit --pretty=oneline
}

function synchronize_kernel_code () { 
	echo "$HOST: Pulling script changes"
	chronic -e git pull --rebase
}

function indent () { 
	if [ $# -eq 1 ]; then
		sed -E "s/^/\t/"
	fi
	"$@" | sed -E "s/^/\t/"
}


operation="$1"
shift
if [ -z "$operation" ]; then
	echo 'Please select: show, sync, build, install, all, clean, '
	echo '               buildonly, installonly, allonly, etc'
	printf '> '
	read -r operation
fi

case "$operation" in 
	'show') 
		show_latest_kernel_commit
		;; 
	'checkout') 
		checkout_kernel_commit "$@"
		;; 
	'cmd') 
		run_command "$@"
		;; 
	'sync') 
		synchronize_kernel_code
		;;
	'all')
		synchronize_kernel_code
		;;
	*)
		# echo 'Unknown option provided to builder script!' 2>/dev/stdout
		# exit 1
		;;
esac
