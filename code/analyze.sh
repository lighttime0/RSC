#!/bin/bash

CMD=$1
ARG=$2
if [[ "$CMD" == "" ]]; then
    echo "usage: $0 [build|run|count]"
    exit 1
fi

if [[ ! -f config ]]; then
    echo "Please create a config according to config.template"
    exit 1
fi

export CURRENT_DIR=`pwd`
TEST_DIR=$CURRENT_DIR/tests
SCRIPT_DIR=$CURRENT_DIR/scripts
SRC_DIR=$CURRENT_DIR/src
source config
export ABS_WORK_DIR=`readlink -f $WORK_DIR`

if [[ ! -f $CURRENT_DIR/rid.so ]]; then
    echo "Please build the Pass module first!"
    exit 1
fi

TARGET=linux.bc

mkdir -p $ABS_WORK_DIR
case $CMD in
    build)
	pushd $KERNEL_DIR > /dev/null
	make CC=clang O=$BUILD_DIR C=1 CHECK="$SCRIPT_DIR/clang-bc-gen" CHECKFLAGS="" -j8 -k
	cd $BUILD_DIR
	find . -name *.bc > $ABS_WORK_DIR/bclist
	popd > /dev/null

	pushd $ABS_WORK_DIR > /dev/null
	# A strange bitcode file in ascii...
	sed -i "/.*hz.bc/d" bclist
	# A duplicate definition that remains...
	sed -i "/.*socket.bc/d" bclist
	# Boot-time code in i386
	sed -i "/.*x86\/realmode.*/d" bclist
	# vdsos
	sed -i "/.*x86\/vdso.*/d" bclist
	# some strange things...
	sed -i "/.*x86\/purgatory.*/d" bclist
	cat bclist | xargs -n 1 -I file -P 8 sh -c 'opt -load $CURRENT_DIR/rid.so -lowerswitch -weaken $1/$2/$3 -o $1/$2/$3.new && mv $1/$2/$3.new $1/$2/$3' -- $KERNEL_DIR $BUILD_DIR file
	echo -n > abs_bclist
	cat bclist | while read f; do echo $KERNEL_DIR/$BUILD_DIR/$f >> abs_bclist; done
	cp $SCRIPT_DIR/inline-list .
	popd > /dev/null
	;;
    prepare)
	pushd $ABS_WORK_DIR > /dev/null
	rm -rf dep.db
	$SCRIPT_DIR/bcdep/depgen.py -d dep.db abs_bclist
	$SCRIPT_DIR/bcdep/markeff.py dep.db
	$SCRIPT_DIR/bcdep/mkgen.py -t $CURRENT_DIR dep.db
	make -f Makefile.scc all -j8
	$SCRIPT_DIR/blackwhitelist-gen
	popd > /dev/null
	;;
    run)
	pushd $ABS_WORK_DIR > /dev/null
	make all -k -j8
	popd > /dev/null
	;;
    report)
	pushd $ABS_WORK_DIR/linux > /dev/null
	mkdir -p reports
	rm -rf reports/*
	find . -name "*.log" | while read f; do
	    if grep "\-\-\-\-\-" $f > /dev/null 2>&1; then
		bn=`basename $f`
		ofn=${bn%.log}.org
		$CURRENT_DIR/scripts/format-report.py -a report.db -p "$KERNEL_DIR/$BUILD_DIR/../" $f > reports/$ofn
	    fi
	done
	popd > /dev/null
	;;
    count)
	pushd $ABS_WORK_DIR > /dev/null
	opt -analyze -quiet -load $CURRENT_DIR/rid.so -o-progress -count $TARGET 2> count.txt | tee count-time.txt
	popd > /dev/null
	;;
    *)
	echo "unknown command: $1"
	;;
esac
