# Door-access-control
A Raspberry Pi (Linux) based door access control prototype with multi-factor authentication,
alarms, and local audit logging.

## Features
- NFC access
- Face verification
- One-time magstripe backup key
- Alarm on authentication failure
- Alarm on forced entry 
- Shared-card alert and Prevent tailing
- Local door access logs 

## Realtime Requirements
- Unlock after verification Response time < 300ms
- Unauthorized door open or More than 3 incorrect keys  alarm: < 300 ms
- Auto re-lock after 5 second


## High-risk scenarios (require face verification)
- Credential reuse within a short time window (anti-passback)
- Repeated authentication failures
- Using the one-time magstripe recovery key
- (Optional) Night mode 

## Hardware (planned)
Raspberry Pi + NFC reader + USB magstripe reader + camera + door reed switch + relay/lock + buzzer + LED.
