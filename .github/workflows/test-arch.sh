#!/bin/bash

run_in_docker() {
    target=.ci/$(basename "$1")
    cat <(echo "cd /pillow") $1 > $target
    chmod +x $target
    echo "*** Starting docker ***"
    docker run --name runner -v $(pwd):/pillow container:latest bash -eo pipefail /pillow/$target
    echo "*** Saving docker session ***"
    docker commit runner container:latest
    docker rm runner
}

run_in_docker $1
