#!/bin/bash
docker build -t aeonos-dev . && \
docker run -it \
    -v $(pwd):/aeonos \
    -v ~/.gitconfig:/root/.gitconfig:ro \
    -v ~/.ssh:/root/.ssh:ro \
    --workdir /aeonos \
    aeonos-dev bash
