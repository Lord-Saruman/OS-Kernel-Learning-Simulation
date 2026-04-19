
MINI OS KERNEL SIMULATOR
Product Requirements Document


Version
1.0 — Initial Release
Course
CS-330: Operating Systems — BESE 30
Instructor
AP Mobeena Shahzad
Date
April 2026
Status
DRAFT

Semester Design Project — Complex Engineering Problem (CEP)

1. Executive Summary
This document defines the product requirements for the Mini OS Kernel Simulator — an interactive, web-accessible simulation platform that models fundamental operating system internals in user space. The system is designed to serve two simultaneous purposes: fulfilling the Complex Engineering Problem (CEP) requirements for CS-330 Operating Systems, and functioning as a publicly usable learning tool for students seeking to understand OS kernel-level concepts through visualization and experimentation.

The simulator is built on a highly modular architecture with a C++ backend engine and a React-based frontend dashboard. It models four core OS subsystems — process and thread management, CPU scheduling, process synchronization, and memory management — each implemented as an independent, interchangeable module.

The core design philosophy is: every OS concept the simulator implements must be visible, explainable, and explorable by the end user in real time.

2. Problem Statement
2.1 Academic Context
Operating systems education traditionally suffers from an abstraction gap: students learn scheduling algorithms, page replacement policies, and synchronization primitives in theory but rarely observe their runtime behaviour in an integrated, realistic environment. Textbook diagrams are static; existing OS simulators are either CLI-only, narrowly scoped, or not designed for interactive exploration.
2.2 Engineering Problem
Design and implement a Mini Operating System Kernel Simulator in user space that:
Models four fundamental OS subsystems: process/thread management, CPU scheduling, process synchronization, and memory management
Supports configurable and swappable policies within each subsystem without requiring changes to other subsystems
Simulates realistic and user-defined workloads
Provides quantitative evaluation of system performance across policies
Behaves like a simplified OS kernel while running entirely as a user-level application

2.3 Learning Tool Gap
There is a need for a publicly accessible, browser-based OS learning platform that allows students to:
Visualize process state transitions and CPU scheduling decisions in real time
Inject custom processes and observe how the scheduler responds
Understand synchronization failures (race conditions) and how primitives prevent them
Experiment with page replacement policies and see page fault rates change

3. Goals and Non-Goals
3.1 Goals
Implement a functionally correct OS kernel simulator covering all four required subsystems
Process and thread lifecycle with PCB/TCB data structures
At least two CPU scheduling algorithms with performance metrics
Mutex and semaphore synchronization primitives
Paging-based memory management with at least one page replacement algorithm
Deliver a modular architecture where each subsystem is independently developable, testable, and replaceable
Provide a web-based React dashboard with real-time visualization of all simulation state
Support both step-by-step and real-time auto-run simulation modes
Allow users to manually inject processes and run prebuilt workload scenarios
Generate quantitative performance metrics and comparative analysis across policy configurations
Design the UI to be self-explanatory and educationally annotated, serving public OS learners
Score Excellent (4/4) across all CEP evaluation rubric criteria

3.2 Non-Goals
This is NOT a real OS kernel — no kernel-mode code, no hardware interrupts, no device drivers
No persistent user accounts or cloud storage of simulation sessions
No support for distributed or multi-machine simulation
No implementation of file systems or network stacks
No mobile-native app (web responsive is sufficient)
No AI-generated content or dynamic curriculum within the learning tool

4. Target Audience
4.1 Primary Users — Public OS Learners
University and college students enrolled in Operating Systems courses who want to visualize and experiment with OS concepts beyond what their textbooks and lectures provide. These users have theoretical knowledge but limited exposure to runtime OS behaviour.

Attribute
Description
Technical Level
Intermediate — understands OS concepts, not necessarily an experienced programmer
Goal
Understand scheduling, synchronization, and paging through interactive experimentation
Pain Point
Existing simulators are CLI-only, hard to configure, or visually unintuitive
Usage Pattern
Load a workload scenario, observe, pause, modify, and compare outputs

4.2 Secondary Users — Academic Evaluators
The course instructor (AP Mobeena Shahzad) and any external evaluators reviewing the project for CEP rubric compliance. They assess the depth of OS implementation, quality of analysis, and communication clarity.

4.3 Tertiary Users — Project Team
The 2-3 person development team who also use the system as a development and debugging harness during implementation. The modularity requirement partly exists to serve this audience.

5. Product Overview
5.1 System Architecture Summary
The system is composed of three layers:

Layer
Technology
Responsibility
Simulation Engine
C++ (multithreaded)
Implements all OS subsystem logic. Real threads internally, externally pausable/controllable.
API Bridge
REST / WebSocket
Exposes simulation state and control commands from the engine to the frontend.
Visualization Dashboard
React (web)
Renders real-time OS state, accepts user commands, displays metrics and educational annotations.

5.2 Four Core Subsystems
Each subsystem is a self-contained module with a defined internal interface. Modules communicate through a shared simulation state object and an event bus — they do not call each other directly.

Module
Core Responsibilities
Process Manager
Process/thread creation and termination, PCB and TCB management, state machine transitions (New → Ready → Running → Waiting → Terminated)
CPU Scheduler
Implements FCFS, Round Robin, and Priority scheduling. Selects next process from ready queue. Reports waiting time, turnaround time, CPU utilization.
Sync Manager
Implements mutex and semaphore primitives. Manages blocked queues. Detects and demonstrates race conditions in critical sections.
Memory Manager
Paging-based virtual memory. Page table management. Implements FIFO and LRU page replacement. Tracks page fault rates.

6. Functional Requirements
6.1 Process & Thread Management
FR-PM-01: The system shall model processes with a minimum of five lifecycle states: New, Ready, Running, Waiting, and Terminated.
FR-PM-02: Each process shall be represented by a Process Control Block (PCB) containing: PID, process name, state, priority, CPU burst time, I/O burst time, remaining burst, arrival time, and page table reference.
FR-PM-03: The system shall support threads within processes, each tracked via a Thread Control Block (TCB) containing: TID, parent PID, state, stack pointer simulation, and register snapshot.
FR-PM-04: Users shall be able to manually create processes via the UI dashboard with configurable parameters.
FR-PM-05: The system shall support prebuilt workload scenarios: CPU-bound, I/O-bound, and mixed.

6.2 CPU Scheduling
FR-SC-01: The system shall implement First-Come-First-Served (FCFS) scheduling.
FR-SC-02: The system shall implement Round Robin (RR) scheduling with configurable time quantum.
FR-SC-03: The system shall implement Priority Scheduling (preemptive and non-preemptive).
FR-SC-04: The scheduler shall be hot-swappable — users can change the scheduling algorithm mid-simulation.
FR-SC-05: The system shall compute and display: average waiting time, average turnaround time, and CPU utilization percentage per scheduling run.
FR-SC-06: A Gantt chart visualization shall be displayed and updated in real time during scheduling.

6.3 Process Synchronization
FR-SY-01: The system shall implement binary and counting semaphores.
FR-SY-02: The system shall implement mutex locks with ownership tracking.
FR-SY-03: The system shall include a built-in race condition demonstration scenario that shows data corruption without synchronization.
FR-SY-04: The system shall include the same scenario with synchronization enabled, showing correct protected execution.
FR-SY-05: Blocked processes waiting on a semaphore or mutex shall be visualized in a blocked queue in the UI.

6.4 Memory Management
FR-MM-01: The system shall simulate a paging-based virtual memory system with configurable page size and frame count.
FR-MM-02: Each process shall have an associated page table shown in the UI.
FR-MM-03: The system shall implement FIFO page replacement.
FR-MM-04: The system shall implement LRU page replacement.
FR-MM-05: The system shall track and display page fault counts and page fault rate per process.
FR-MM-06: Users shall be able to switch between FIFO and LRU mid-simulation and observe the impact.

6.5 Simulation Control
FR-SIM-01: The simulation shall support a Step Mode where each clock tick advances only on user command.
FR-SIM-02: The simulation shall support an Auto Mode where ticks advance at a configurable speed.
FR-SIM-03: The simulation shall be pausable, resumable, and resettable at any point.
FR-SIM-04: Simulation speed in Auto Mode shall be adjustable via a slider (slow / normal / fast).

6.6 Learning Tool Features
FR-LT-01: Each subsystem panel in the UI shall include an expandable concept explanation tooltip.
FR-LT-02: Every scheduling decision shall be annotated with a plain-English reason in a decision log panel.
FR-LT-03: The system shall provide a side-by-side comparison view for any two scheduling or memory policies.
FR-LT-04: Performance metrics shall be displayed in both tabular and chart form.

7. Non-Functional Requirements

ID
Category
Requirement
NFR-01
Modularity
Each OS subsystem must be independently compilable and testable without dependencies on other subsystems.
NFR-02
Extensibility
Adding a new scheduling algorithm must require zero changes outside the Scheduler module.
NFR-03
Performance
The UI must render simulation state updates within 100ms of each clock tick in Auto Mode.
NFR-04
Usability
A new user with OS theory knowledge must be able to run a full simulation scenario within 5 minutes of opening the dashboard with zero documentation.
NFR-05
Correctness
All scheduling metric calculations must match manually computed reference values within a 1% margin.
NFR-06
Accessibility
The web dashboard must be functional on Chrome and Firefox on desktop browsers.
NFR-07
Concurrency Safety
The C++ engine must use real OS threads with proper synchronization. No data races shall be permitted in the engine itself.

8. Constraints & Assumptions
8.1 Technical Constraints
The simulator must run entirely in user space — no kernel modules, kernel-mode code, or root privileges
The C++ engine is the single source of truth for simulation state; the React frontend is display-only
All four subsystems defined in the CEP must be implemented — none can be omitted
Minimum two scheduling algorithms and one page replacement algorithm (CEP mandate)

8.2 Project Constraints
Team size: 2-3 members
Timeline: 4-6 weeks from project start to final submission
Academic submission: CEP rubric compliance is a hard requirement, not optional

8.3 Assumptions
The team has strong working knowledge of OS theory across all four subsystems
The team has sufficient C++ and React experience to implement without framework ramp-up
All team members have access to a development environment capable of running the C++ backend and React frontend locally
No budget is required — all tools and frameworks used are open source

9. Success Metrics

Metric
Target
How Measured
CEP Rubric Score
Excellent (4/4) across all criteria
Instructor evaluation
Scheduling Metric Accuracy
≤1% deviation from manual calculation
Test plan reference values
Module Independence
Each module passes unit tests in isolation
Isolated module test suite
Page Fault Correctness
FIFO and LRU outputs match textbook examples
Reference trace test cases
UI Learnability
New user runs scenario in under 5 minutes
Peer usability test
Race Condition Demo
Visibly shows corruption without sync, clean with sync
Visual + log verification

10. Out of Scope
The following items are explicitly out of scope for version 1.0 of this project:
File system simulation
Network stack or socket simulation
Disk I/O scheduling (e.g. SSTF, SCAN)
Virtual machine or hypervisor simulation
User authentication or persistent session storage
Mobile native application
Distributed or multi-node simulation
Real hardware interaction of any kind

These may be considered for future enhancement versions beyond the semester scope.

11. Glossary

Term
Definition
PCB
Process Control Block — data structure storing all metadata about a process in the OS kernel
TCB
Thread Control Block — data structure storing metadata about a single thread within a process
FCFS
First-Come-First-Served — CPU scheduling algorithm that executes processes in arrival order
RR
Round Robin — CPU scheduling algorithm that assigns fixed time slices (quanta) to each process in rotation
Semaphore
Synchronization primitive using a counter to control access to shared resources
Mutex
Mutual Exclusion lock — binary synchronization primitive ensuring only one thread accesses a critical section at a time
Paging
Memory management scheme that divides virtual and physical memory into fixed-size blocks (pages and frames)
LRU
Least Recently Used — page replacement policy that evicts the page not used for the longest time
FIFO
First-In-First-Out — page replacement policy that evicts the oldest loaded page
CEP
Complex Engineering Problem — the classification of this semester project under FAST-NUCES academic framework
User Space
Execution environment for normal applications, as opposed to kernel space where the OS runs with full hardware privileges
Gantt Chart
Visual timeline showing which process occupied the CPU during each time unit

