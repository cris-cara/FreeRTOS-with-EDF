# FreeRTOS-with-EDF
Usage of FreeRTOS with a practical example and implementation of the Earliest Deadline First Scheduling Algorithm.

## How to Get Started
This project is organized into three branches, each focusing on a specific topic related to context of FreeRTOS.

## Available Branches

### 1. develop

This branch contains a practical example on the usage of FreeRTOS for simulated monitoring of an automated greenhouse. FreeRTOS, which is a real-time operating system for embedded microcontrollers, is used here to manage the automated workflow inside the greenhouse.

### 2. edf-scheduler

This branch focuses on the implementation of Earliest Deadline First (EDF) scheduling. In this context, the EDF scheduling algorithm is used to ensure that tasks are executed based on their closest deadline, optimizing time adherence.

### 3. rr-scheduler

This branch concentrates on the implementation of Fixed-Priority Scheduling with Time Slicing, which is the default scheduling algorithm of FreeRTOS. In this configuration, tasks are assigned fixed priorities, but the concept of time slicing is also utilized to ensure that tasks with similar priorities have fair opportunities to execute their code (Round-Robin mode).
