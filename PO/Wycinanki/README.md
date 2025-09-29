# Matrix & Array Operations Library 🧮

## Overview
This project is an **object-oriented implementation of multi-dimensional numeric arrays** (scalars, vectors, and matrices) in Java.  
It was developed as part of the **Object-Oriented Programming** course to practice **inheritance, polymorphism, operator-like methods, and exception handling**.

The library provides a foundation for mathematical computation, supporting both **immutable and mutable operations** on arrays of dimension 0, 1, and 2.

---

## Key Features
- **Data structures:**
  - `Skalar` (0D) – single numeric value  
  - `Wektor` (1D) – horizontal or vertical vector  
  - `Macierz` (2D) – rectangular matrix  

- **Core operations:**
  - Addition, multiplication, negation (with both *new result* and *in-place* variants)  
  - Scalar operations (e.g., add scalar to every element)  
  - Vector–matrix interactions (dot product, outer product)  
  - Assignment between compatible shapes (scalar → vector/matrix, vector → matrix, etc.)  

- **Indexing & slicing:**
  - Direct element access with `daj` (get) and `ustaw` (set)  
  - Multi-dimensional indexing using varargs  
  - Array slices (`wycinek`) that behave as live views of the original data  

- **Additional utilities:**
  - Shape, dimension, and element count queries  
  - Deep copy (`kopia`)  
  - Transposition (`transponuj`)  
  - `toString()` with human-readable representation  

- **Robust exception handling:**
  - Custom checked exceptions for invalid operations  
  - Clear separation between recoverable argument errors and fatal logic errors  

---

## Technical Highlights
- **Language**: Java  
- **Design principles**: OOP, inheritance, method overriding & overloading  
- **Focus areas**:  
  - Polymorphism in mathematical structures  
  - Operator-like methods (`suma`, `iloczyn`, `zaneguj`, etc.)  
  - Exception safety and validation  
- **Data structures**: implemented with plain Java arrays (no external collections)  

---

## Example Use Cases
- Perform vector and matrix arithmetic with clean OOP design  
- Explore slices of arrays and operate directly on them  
- Demonstrate polymorphism (`Skalar`, `Wektor`, `Macierz` share common API)  
- Serve as a **mini linear algebra engine** in pure Java  

