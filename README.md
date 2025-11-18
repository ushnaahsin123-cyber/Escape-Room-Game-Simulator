# Escape Room Simulator

A text-based escape room adventure game written in C++ where players solve puzzles, collect items, and explore rooms to escape.

## Description

This is a console-based escape room game where players navigate through multiple rooms, solve puzzles, inspect objects, and collect items to progress. The game features save/load functionality, high score tracking, multiple difficulty levels, and achievement system.

## Features

- Multiple rooms with interactive objects
- Puzzle-solving mechanics (numeric and text-based)
- Inventory system for collecting and using items
- Save/Load game progress
- High score tracking with player names
- Difficulty levels (Easy, Medium, Hard)
- Achievement system
- Randomized puzzles for replayability
- Game summary with statistics

## Installation & Compilation

### Requirements
- C++ compiler (GCC, Clang, or MSVC)
- Standard C++ libraries

**Game Files**
main.cpp - Main game source code
rooms.txt - Room and puzzle definitions
savegame.dat - Save game data (auto-generated)
highscores.dat - High scores data (auto-generated)

**How to Play**
Start Game: Choose from main menu
Navigate Rooms: Solve all puzzles in each room to proceed
Interact with Objects: Select objects by number to inspect
Solve Puzzles: Answer questions or solve numeric puzzles
Use Inventory: Collect and use items with 'I' command
Save Progress: Use 'S' to save your game
Quit: Use 'Q' to return to main menu

**Controls**
Numbers 1-9: Select objects in current room
I: View and use inventory
S: Save game progress
Q: Quit to main menu

**Project Structure**
escape-room/
├── main.cpp          # Main game source code
├── rooms.txt         # Room and object definitions
├── savegame.dat      # Save files (auto-generated)
├── highscores.dat    # High scores (auto-generated)
└── README.md         # This file

**Credits**
Created for C++ Programming Course
Uses standard C++ libraries
Built with object-oriented design principles

**Date**
18/11/2025
