FROM alpine AS build
MAINTAINER chriseth <chris@ethereum.org>
#Official hyperion docker image

#Establish working directory as hyperion
WORKDIR /hyperion

# Build dependencies
RUN apk update && apk add boost-dev boost-static build-base cmake git

#Copy working directory on travis to the image
COPY / $WORKDIR

# Number of parallel jobs during build
# or 0 for auto-computing (max(1, CPU_core_count * 2/3), a greedy value)
ARG BUILD_CONCURRENCY="0"

#Install dependencies, eliminate annoying warnings
RUN sed -i -E -e 's/include <sys\/poll.h>/include <poll.h>/' /usr/include/boost/asio/detail/socket_types.hpp
RUN cmake -DCMAKE_BUILD_TYPE=Release -DTESTS=0 -DHYPC_LINK_STATIC=1
RUN make hypc \
    -j$(awk "BEGIN {                                       \
        if (${BUILD_CONCURRENCY} != 0) {                   \
            print(${BUILD_CONCURRENCY});                   \
        } else {                                           \
            x=($(grep -c ^processor /proc/cpuinfo) * 2/3); \
            if (x > 1) {                                   \
                printf(\"%d\n\", x);                       \
            } else {                                       \
                print(1);                                  \
            }                                              \
        }                                                  \
    }")
RUN strip hypc/hypc

FROM scratch
COPY --from=build /hyperion/hypc/hypc /usr/bin/hypc
ENTRYPOINT ["/usr/bin/hypc"]
