#!/bin/bash

function init () {
  mkdir -p src/ include/ data/ logs/ reports/ tmp/obj tests/ doc/ tools/
  if [ -z ${CC+x} ]; then
    CC=gcc
  fi

  if [ -z ${CFLAGS+x} ]; then
    CFLAGS="-std=c11 -Wall -Wextra -Werror"
  fi

  if ! command -v $CC &> /dev/null; then
      echo "error: $CC program not found" >&2
      log_exit 1
  else
      log "Initialised project"
  fi
}


function compile_obj () {
    log_cmd $CC $CFLAGS -c -o $2 $1
}

function build() {
    DIR=${1%/}

    if [ $# -eq 0 -o -z $DIR ]; then
        return
    fi

    if [ -d $1 ]; then
        for dir in $DIR/*; do
            build ${dir}
        done
        return
    fi

    SRC_FILE=$1

    if ! [ -f $SRC_FILE -a "${SRC_FILE##*.}" == "c" ]; then
        return
    fi

    OBJ_FILE=./tmp/obj/$(basename $SRC_FILE .c).o

    if [ -f $OBJ_FILE ]; then
        if [ $SRC_LAST_MODIFIED -nt $OBJ_LAST_MODIFIED ]; then
            compile_obj "$SRC_FILE" "$OBJ_FILE"
            if [ -z $SILENT ]; then
                echo "Recompiled $SRC_FILE -> $OBJ_FILE"
            fi
        fi
    else 
        compile_obj "$SRC_FILE" "$OBJ_FILE"
        if [ -z $SILENT ]; then
            echo "Compiled $SRC_FILE -> $OBJ_FILE"
        fi
    fi


    SRC_NAME=${SRC_FILE##*/}
    if [ "${SRC_NAME%%_*}" = "main" ]; then
        if [ -z $EXECUTABLES ]; then
            EXECUTABLES=${OBJ_FILE}
        else
            EXECUTABLES="${EXECUTABLES} ${OBJ_FILE}"
        fi
    else
        OBJECTS="$OBJECTS $OBJ_FILE"
    fi
}

function print_help () {
    if [ $# -eq 0 ]; then
        echo ""
        echo "FileOps Usage:"
        echo " fileops.sh init [--silent] - Creates the project directories and checks for C compiler"
        echo " fileops.sh run -- <executable> [args...] - Runs the executable in ./bin/ forwarding the arguments to it"
        echo " fileops.sh test [--silent] - Runs tests in ./tests/"
        echo " fileops.sh build [--src dir] [--silent] - Builds all of the source files in the 'dir' directory. If 'dir' is not specified then it defaults to ./src/"
        echo " fileops.sh clean [--silent] - Cleans up the created artifacts"
        echo " fileops.sh help  [command]  - Shows a more detailed description about a command"
        echo ""
    else
        case $1 in
            "init")
                echo ""
                echo "fileops.sh init: "
                echo " Creates src/ include/ data/ logs/ reports/ tmp/obj tests/ doc/ tools/ if they don't exist already."
                echo " Checks for 'gcc' or any 'CC' already set environment variable"
                echo ""
            ;;
            "run")
                echo ""
                echo "fileops.sh run -- <executable> [args...]:"
                echo " Runs executable in ./bin/"
                echo " Forwards any arguments in args to the executable"
                echo ""
            ;;
            "build")
                echo ""
                echo "fileops.sh build [--src dir] [--silent]: "
                echo " Builds the source files located in './dir/'."
                echo " The building goes like this:"
                echo "    - It recursively searches in all subfolders and the current folder for files that end in .c"
                echo "    - It incrementally compiles into objects all .c files that do not have a .o object in ./tmp/obj or .c files that have been modified"
                echo "    - Separates the files that start with main_ to use them as entry points to build executables"
                echo "    - All the other files that do not start with main_ will be used as dependencies for all other main_ executables"
                echo "    - Lastly it generates all the executables in ./bin/ stripped from their main_ prefix"
                echo " Default value for the src folder option is 'src'."
                echo " Environment Variables can be configured like this:"
                echo "    - CC -> The compiler that the system will use to compile all sources and object files into executables (default: gcc)"
                echo "    - CFLAGS -> The C flags that the system is gonna specify to the compiler everytime it compiles a source or objects into executables (default: -std=c11 -Wall -Wextra -Werror)"
                echo ""
            ;;
            "clean")
                echo ""
                echo "fileops.sh clean [--silent]:"
                echo " Removes all artifacts in ./bin/ and ./tmp/"
                echo ""
            ;;
            "test")
                echo ""
                echo "fileops.sh test [--silent]:"
                echo " Runs all test .sh scripts in ./tests/"
                echo " Generates ./reports/T2_tests.txt containing the passed and failed tests"
                echo " If at least one test fails this script returns a non-zero exit code"
                echo ""
            ;;
            *)
                print_help
            ;;
        esac
    fi
}

function log_cmd () {
    cmd=$*
    
    log $cmd
    eval $cmd
}

function log () {
    if [ -z $SILENT ]; then
        echo $*
    fi
}


function process_options () {
    while [ $# -gt 0 ]; do
        case $1 in
            "--src")
                if [ -z $2 -o -z ${2##-*} ]; then
                    print_help
                    log_exit 0
                fi
                SRC_DIR=$2
                shift 1
            ;;
            "--silent")
                export SILENT=1
            ;;
        esac
        shift 1
    done
}

function log_exit () {
    END=$(date --rfc-3339=ns)
    START_NS=$(date -d"$START" +%s%N)
    END_NS=$(date -d"$END" +%s%N)

    let DIFF=($END_NS-$START_NS)/1000000

    code=$1
    date=$(date -d"$START" +%Y%m%d_%H%M%S_%N)
    name="fileops_$date.log"
    log="./logs/$name"

    if [ -z $subcmd ]; then
        subcmd="None"
    fi

    echo "SubCommand: $subcmd" > $log
    echo "Start Time: $START" >> $log
    echo "End Time: $END" >> $log
    echo "Runtime: ${DIFF}ms" >> $log
    echo "Exit Code: $code" >> $log

    exit $code
}

START=$(date --rfc-3339=ns)
subcmd=$1

if [ $# -eq 0 ]; then
    print_help
    log_exit 0
fi

case $subcmd in 
    "init")
        shift 1
        process_options "$@"
        init
    ;;
    "run")
        shift 1
        if [ "$1" != "--" ]; then
            print_help
            log_exit 0
        fi

        shift 1

        if [ ! -f "./bin/$1" ]; then
            echo "error: executable $1 does not exit" >&2
            log_exit 1
        fi

        eval ./bin/$@ 
    ;;
    "build")
        shift 1
        SRC_DIR=src
        process_options "$@"
        init &> /dev/null
        build $SRC_DIR

        if [ ! -d $SRC_DIR ]; then
            echo "error: '$SRC_DIR' does not exist or is not a directory" >& 2
            log_exit 1
        fi
        
        for executable_obj in $EXECUTABLES; do
            executable=$(basename $executable_obj .o)
            executable=${executable#main_*}
            log_cmd $CC $CFLAGS -o ./bin/$executable $executable_obj $OBJECTS
        done
    ;;
    "test")
        process_options "$@"
        exit_code=0

        log "Executing tests..."
        log ""


        rm -f ./reports/T2_tests.txt
        for test in $(find -type f -path "./tests/**.sh"); do
            bash $test

            last=$?

            if [ $last -ne 0 ]; then
                log "Test $test - FAILED"
                echo "$test - FAIL" >> ./reports/T2_tests.txt
                exit_code=1
            else
                log "Test $test - PASSED"
                echo "$test - PASS" >> ./reports/T2_tests.txt
            fi
            log ""
        done
        log ""

        log_exit $exit_code

    ;;
    "clean")
        process_options "$@"

        log_cmd rm -rf ./bin/*
        log_cmd rm -rf ./tmp/obj
    ;;
    "help")
        shift 1
        print_help $1
    ;;
    *)
        print_help
    ;;
esac

log_exit 0



