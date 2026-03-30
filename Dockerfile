# Una sola etapa para depurar y asegurar dependencias
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Instalar dependencias esenciales y habilitar Universe
RUN apt-get update && apt-get install -y \
    software-properties-common \
    && add-apt-repository -y universe \
    && apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libssl-dev \
    libsasl2-dev \
    pkg-config \
    curl \
    libmongocxx-dev \
    libbsoncxx-dev \
    && rm -rf /var/lib/apt/lists/*

# Instalar Node.js
RUN curl -fsSL https://deb.nodesource.com/setup_20.x | bash - \
    && apt-get install -y nodejs

WORKDIR /app
COPY . .

# Compilar binarios de Fuxion
RUN g++ -std=c++17 mongo_benchmark.cpp -o mongo_benchmark $(pkg-config --cflags --libs libmongocxx libbsoncxx)
RUN g++ -std=c++17 cierre_fuxion.cpp -o cierre_fuxion $(pkg-config --cflags --libs libmongocxx libbsoncxx)

# Instalar dependencias de Node
RUN npm install

EXPOSE 3000

CMD ["npm", "start"]
