FROM nexus3.o-ran-sc.org:10002/o-ran-sc/bldr-ubuntu18-c-go:1.9.0 as rcbuild

ENV PATH=/opt/go/1.14/bin:$PATH
ENV PATH $PATH:/usr/local/bin
ENV GOPATH /go
ENV GOBIN /go/bin
ENV CFG_FILE /opt/ric/config/config-file.json
ENV RMR_SEED_RT /opt/routes.txt

COPY routes.txt /opt/routes.txt

ARG RMRVERSION=4.7.4
ARG RMRLIBURL=https://packagecloud.io/o-ran-sc/release/packages/debian/stretch/rmr_${RMRVERSION}_amd64.deb/download.deb
ARG RMRDEVURL=https://packagecloud.io/o-ran-sc/release/packages/debian/stretch/rmr-dev_${RMRVERSION}_amd64.deb/download.deb
RUN wget --content-disposition ${RMRLIBURL} && dpkg -i rmr_${RMRVERSION}_amd64.deb
RUN wget --content-disposition ${RMRDEVURL} && dpkg -i rmr-dev_${RMRVERSION}_amd64.deb
RUN rm -f rmr_${RMRVERSION}_amd64.deb rmr-dev_${RMRVERSION}_amd64.deb

ARG XAPPFRAMEVERSION=v0.8.3
WORKDIR /go/src/gerrit.o-ran-sc.org/r/ric-plt
RUN git clone -b ${XAPPFRAMEVERSION} "https://gerrit.o-ran-sc.org/r/ric-plt/xapp-frame"
RUN cd xapp-frame && \
    GO111MODULE=on go mod vendor -v && \
    cp -r vendor/* /go/src/ && \
    rm -rf vendor

WORKDIR /go/src/gerrit.o-ran-sc.org/r/ric-app/rc
COPY go.mod .
COPY cmd cmd
COPY control control
COPY e2ap e2ap
COPY e2sm e2sm
COPY xapp-descriptor xapp-descriptor

# "COMPILING E2AP Wrapper"
RUN cd e2ap && \
    gcc -c -fPIC -Iheaders/ lib/*.c wrapper.c && \
    gcc *.o -shared -o libe2apwrapper.so && \
    cp libe2apwrapper.so /usr/local/lib/ && \
    mkdir /usr/local/include/e2ap && \
    cp wrapper.h headers/*.h /usr/local/include/e2ap && \
    ldconfig

# "COMPILING E2SM Wrapper"
RUN cd e2sm && \
    gcc -c -fPIC -Iheaders/ lib/*.c wrapper.c && \
    gcc *.o -shared -o libe2smwrapper.so && \
    cp libe2smwrapper.so /usr/local/lib/ && \
    mkdir /usr/local/include/e2sm && \
    cp wrapper.h headers/*.h /usr/local/include/e2sm && \
    ldconfig

RUN go build ./cmd/rc.go && pwd && ls -lat

FROM ubuntu:18.04
ENV CFG_FILE /opt/ric/config/config-file.json
ENV RMR_SEED_RT /opt/routes.txt

COPY routes.txt /opt/routes.txt
RUN apt update && apt install ca-certificates libgnutls30 nano -y
COPY --from=rcbuild /usr/local/lib /usr/local/lib
RUN ldconfig
WORKDIR /go/src/gerrit.o-ran-sc.org/r/ric-app/rc
COPY --from=rcbuild /go/src/gerrit.o-ran-sc.org/r/ric-app/rc/rc .
CMD ./rc
