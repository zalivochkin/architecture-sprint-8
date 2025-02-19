FROM gcc:14.2.0 as build

RUN apt update && \
    apt install -y pipx cmake
RUN pipx ensurepath
RUN pipx install conan==2.12.2
ENV PATH="/root/.local/bin:${PATH}"
RUN conan profile detect

WORKDIR /app
COPY conanfile.txt ./

WORKDIR /app/build
RUN conan install --build=missing -s compiler.version=14.2 -s compiler.libcxx=libstdc++11 -s build_type=Release ..

ADD ./CMakeLists.txt /app/CMakeLists.txt
ADD ./src/ /app/src/

RUN cd /app/build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake && \
    cmake --build .

FROM ubuntu:latest

RUN apt update
RUN apt install -y libcurl4-openssl-dev

WORKDIR /app

COPY --from=build /app/build/api_service .

ENTRYPOINT ["/app/api_service"]
