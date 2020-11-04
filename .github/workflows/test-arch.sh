#!/bin/bash

run_in_docker() {
    target=.ci/$(basename "$1")
    cat <(echo "cd /pillow") $1 > $target
    chmod +x $target
    echo "*** Starting docker ***"
    docker run --name runner -v $(pwd):/pillow ${@:2} container:latest bash -eo pipefail /pillow/$target
    echo "*** Saving docker session ***"
    docker commit runner container:latest
    docker rm runner
}

# test-arch.yml will append a line in the following format:
# run_in_docker $1 -e GITHUB_ACTIONS=true -e GITHUB_SHA=abcdef ...
