# 📨 Publish-and-Subscribe Middleware Architecture

This project is a simple yet scalable **Publish-Subscribe (Pub/Sub)** middleware system developed using **Client-Server Socket Programming in C**. It demonstrates asynchronous message exchange between **Publishers** and **Subscribers** through a central server that handles **concurrent client connections** and **topic-based message routing**.

---

## 📚 Project Overview

This system implements a lightweight Pub/Sub architecture:

- Clients can act as either **Publishers** or **Subscribers**.
- Publishers send messages associated with specific **topics**.
- Subscribers receive messages **only for the topics they subscribed to**.
- The server supports **multiple concurrent clients** using **multithreading**.
- Implements **message filtering by topics**, so only relevant messages are forwarded.

---

## 🚀 Features

### ✅ Basic Publish-Subscribe Functionality 

- Handles **multiple concurrent clients** using threads.
- Clients specify their role via CLI argument (`PUBLISHER` or `SUBSCRIBER`).
- Publishers can send messages to the server.
- The server forwards publisher messages **only to subscriber clients**.
- Publishers **do not receive** messages from other publishers or themselves.

### ✅ Topic-Based Messaging 

- Adds **topic-based filtering** to Pub/Sub messaging.
- Clients specify a **topic name** when connecting via CLI.
- Messages from publishers are **routed only to subscribers** with the **matching topic**.
- Supports **multiple topics concurrently** with multiple publishers and subscribers.

---

## 💻 Compilation

```bash
gcc server.c -o server -lpthread
gcc client.c -o client
