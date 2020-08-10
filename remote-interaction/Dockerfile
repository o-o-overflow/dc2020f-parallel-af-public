FROM python:3.7-alpine

ENV TERM linux

run apk update && apk add --no-cache netcat-openbsd bash

COPY requirements.txt /requirements.txt
RUN pip install -r requirements.txt

COPY exploit_phase2.py /exploit_phase2.py
COPY os /os
COPY check.py /check.py


