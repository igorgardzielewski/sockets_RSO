FROM ubuntu:latest
RUN apt-get update && apt-get install -y \
    gcc \
    libc6-dev \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY client.c client_config.txt ./

RUN gcc -Wall -Wextra -o client client.c -lm

CMD ["./client"]