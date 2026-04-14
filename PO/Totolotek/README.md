# Totolotek Lottery Simulator 🎲

## Overview
This project is an **object-oriented simulation** of the Polish lottery system "Totolotek", implemented as part of the **Object-Oriented Programming 2024L** course.  

It models the full ecosystem of a lottery:
- 🎰 Central lottery office (**Centrala**)  
- 🏪 Ticket vendors (**Kolektury**)  
- 🧾 Tickets and betting slips (**Kupony**, **Blankiety**)  
- 👤 Different types of players with unique strategies  
- 💰 Prize distribution, taxation, and state budget interaction  

The focus is on **object-oriented design**: encapsulation, inheritance, polymorphism, state management, and test-driven development with **JUnit**.

---

## Key Features
- **Lottery draws** – automatic random draws of 6 numbers out of 49, with rolling jackpots (cumulative prizes).  
- **Prize calculation** – winnings for 3, 4, 5, or 6 matches, with guaranteed minimums and taxation rules.  
- **Ticket system** – tickets can include multiple bets and multiple draws.  
- **Multiple player strategies** – minimalists, random players, fixed-number players, and fixed-slip players.  
- **Budget simulation** – models both the central lottery funds and the government budget (taxes + subsidies).  
- **Extensible design** – new player types or business rules can be added easily.  

---

## Technical Highlights
- **Language**: Java  
- **Testing**: JUnit  
- **Design principles**: OOP, modularity, separation of concerns  
- **Data flow**: Players → Kolektura → Centrala → Budget  

---

## Example Flow
1. Create a lottery central and 10 vendors.  
2. Generate 200 players of each type.  
3. Simulate 20 consecutive draws with ticket purchases.  
4. Players automatically check and collect winnings.  
5. Print reports: draw history, payouts, taxes collected, subsidies received.  

