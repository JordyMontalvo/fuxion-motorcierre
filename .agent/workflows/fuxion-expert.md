---
description: Experto en el Plan de Pagos FuXion (Pro-Lev X) para el cálculo de Rangos y Bono Residual.
---

Este flujo de trabajo define las reglas lógicas para el cálculo de compensación de FuXion, específicamente para el sistema de alto rendimiento en C++.

# 1. Glosario de Términos Clave
- **PV4 (Volumen Personal de 4 Semanas):** Suma del volumen de calificación (QV) de las compras personales del EF en las últimas 4 semanas.
- **DV4 (Volumen Grupal de 4 Semanas):** Suma del QV de toda la organización (Familia FuXion) en las últimas 4 semanas.
- **MVR (Regla del Máximo Volumen):** Tope máximo de puntos que una sola línea de patrocinio puede aportar al DV4 para calificar a un rango.
- **Línea 1K:** Una línea de patrocinio que genera al menos 1,000 DV4.
- **Familia FuXion (FF):** Árbol de patrocinio (unilevel).

# 2. Matriz de Requisitos de Rangos
Para calificar a un rango, el EF debe cumplir simultáneamente: PV4, DV4 (aplicando MVR por línea) y Líneas Calificadas.

| Rango | PV4 | DV4 Total | Máx por Línea (MVR) | Requisito de Líneas |
| :--- | :--- | :--- | :--- | :--- |
| **Entrepreneur** | 40 | - | - | - |
| **Executive Entr.** | 100 | 500 | 300 | - |
| **Senior Entr.** | 100 | 1,000 | 600 | - |
| **Team Builder** | 150 | 2,000 | 1,200 | - |
| **Senior Team Builder** | 150 | 4,000 | 2,400 | 1 Línea 1K |
| **Leader X** | 200 | 6,000 | 3,600 | 2 Líneas 1K |
| **Premier Leader** | 200 | 15,000 | 9,000 | 2 Líneas 1K + 1 Leader X |
| **Elite Leader** | 200 | 30,000 | 18,000 | 1 Línea 1K + 2 Leader X |
| **Diamond** | 200 | 60,000 | 30,000 | 4 Leader X |
| **Blue Diamond** | 200 | 100,000 | 50,000 | 4 Premier Leader |

# 3. Bono Residual Básico (Bono Familia ["X"])
Este bono paga un porcentaje del Volumen Comisionable (CV) de la Familia FuXion hasta 6 niveles de profundidad, basado en el **Rango Pagado** de la semana.

### Tabla de Porcentajes por Rango y Nivel:
| Nivel | Entrepreneur | Executive | Senior | Team Builder | Senior TB | Leader X+ |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Nivel 1** | 5% | 6% | 7% | 8% | 9% | 10% |
| **Nivel 2** | - | 3% | 4% | 5% | 6% | 7% |
| **Nivel 3** | - | - | 3% | 4% | 5% | 6% |
| **Nivel 4** | - | - | - | 3% | 4% | 4% |
| **Nivel 5** | - | - | - | - | 3% | 3% |
| **Nivel 6** | - | - | - | - | - | 2% |

### Regla de Compresión Dinámica:
Si un EF en un nivel intermedio no está activo (menos de 40 PV4), el sistema "comprime" los niveles hacia arriba para que el patrocinador reciba el bono de los niveles inferiores activos dentro de sus niveles de pago permitidos.

# 4. Implementación en C++ (Pseudo-Lógica)
Para procesar 10 millones de usuarios eficientemente:
1. **Paso 1: Agregación de Volumen.** Recorrer el árbol (post-order) para calcular el DV4 real de cada nodo y almacenar el volumen por línea.
2. **Paso 2: Evaluación de Rango.** Por cada usuario, verificar PV4, sumar DV4 (truncando cada línea al MVR del rango objetivo) y verificar si sus hijos cumplen los rangos de línea requeridos.
3. **Paso 3: Cálculo del Bono.** Identificar el Rango Pagado, y aplicar el multiplicador (5%-10%) sobre el CV de los documentos en sus primeros 6 niveles de Familia.

// turbo-all
