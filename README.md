# Uni Coursework

My coursework from the BSc Software Engineering at Asia Pacific University (APU), Kuala Lumpur. Each project sits in its own folder.

## DSTR Parcel System (C++)

Data Structures assignment (Assignment 2), done as a team. One integrated C++ prototype that manages parcels from arrival to completion. The five modules share and update the same data:

1. Parcel intake, sorting and processing
2. Parcel service and collection management
3. Collection counter and delivery resource management
4. Parcel activity and record management
5. Parcel category organisation (tree based)

All data structures are implemented by hand. The STL containers (vector, list, stack, queue, priority_queue, deque) were not allowed.

### Data structures used
- Queue: IntakeQueue.hpp, parcels waiting to be sorted and processed
- Priority queue (heap): RequestHeap.hpp, service and collection requests
- Circular queue: CircularQueue.hpp, collection counter and delivery resources
- Stack: ActivityStack.hpp, parcel activity history (my part)
- Binary search tree: ParcelBST.hpp, organising parcels by category
- Dynamic array: DynArray.hpp, resizable storage

### My part
Module 4, Parcel Activity and Record Management (ActivityModule.cpp, ActivityModule.hpp, ActivityStack.hpp). I implemented the stack from scratch and built the module around it to record parcel activity, with the most recent entries accessible first.

Why a stack: activity records are read newest first, which is last in, first out, and push and pop both run in constant time.

### File layout
- main.cpp and System.hpp: entry point and the wiring between modules
- IntakeModule, RequestModule, ResourceModule, ActivityModule: one per module
- Types.hpp, Utils: shared definitions and helpers
- sample_parcels.csv: test data

### Build and run
From inside DSTR_Parcel_System:

    g++ -o parcel main.cpp IntakeModule.cpp RequestModule.cpp ResourceModule.cpp ActivityModule.cpp Utils.cpp
    ./parcel

The compiled binary is gitignored, so it has to be rebuilt after cloning.

## VarQuest Web App (Node.js)

A web quiz game for learning Python concepts. Players go through rounds of questions, reveal the answer after each one, and see their results at the end.

- Server: Node.js with Express (app.js)
- Database: database.js, with linked Question and Round tables
- Views: Pug templates for start, question, reveal and results (views folder)
- Styling: public/style.css

### Run
From inside VarQuest_Web_App:

    npm install
    node app.js

Then open the address printed in the terminal.

## Notes
- This repo was assembled in November 2026 from separate project folders. VarQuest used to live in its own repo, VarQuest-web-app.
