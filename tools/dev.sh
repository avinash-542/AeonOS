#!/bin/bash
docker build -t aeonos-dev . && \
docker run -it \
    -v $(pwd):/aeonos \
    --workdir /aeonos \
    aeonos-dev bash -c "git config core.hooksPath .githooks && bash"
