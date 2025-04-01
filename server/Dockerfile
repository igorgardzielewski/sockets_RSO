FROM ubuntu:latest
RUN apt-get update && apt-get install -y \
    gcc \
    libc6-dev \
    make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY server.c server_config.txt ./
RUN gcc -Wall -Wextra -o server server.c -lm -pthread

CMD ["./server"]