FROM alpine:3.6

RUN apk update
RUN apk add gcc g++ make git gnutls-dev gnutls-c++ clang