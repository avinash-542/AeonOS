#!/bin/bash
docker build -t aeonos-dev . && \
docker run -it \
    -v $(pwd):/aeonos \
    -v ~/.gitconfig:/root/.gitconfig:ro \
    -v ~/.ssh:/root/.ssh:ro \
    --workdir /aeonos \
    aeonos-dev bash -c "git remote set-url origin git@github.com:avinash-542/AeonOS.git && git config --local push.autoSetupRemote true && bash"