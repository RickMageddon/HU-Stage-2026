# Alternative Mermaid Diagram Examples

Since the original picture was not available, here are several common diagram patterns you can use. Choose or modify the one that best matches your system architecture.

## Example 1: Simple Control Flow

```mermaid
graph LR
    A[Input] --> B[Controller]
    B --> C[Processor]
    C --> D[Output]
```

## Example 2: Microservices Architecture

```mermaid
graph TD
    API[API Gateway] --> Auth[Auth Service]
    API --> User[User Service]
    API --> Product[Product Service]
    API --> Order[Order Service]
    
    User --> UserDB[(User DB)]
    Product --> ProductDB[(Product DB)]
    Order --> OrderDB[(Order DB)]
    
    Order --> MessageQueue[Message Queue]
    MessageQueue --> Notification[Notification Service]
```

## Example 3: IoT/Hardware Control System

```mermaid
graph TD
    WebApp[Web Application] --> CloudAPI[Cloud API]
    CloudAPI --> Gateway[IoT Gateway]
    Gateway --> Sensor1[Temperature Sensor]
    Gateway --> Sensor2[Humidity Sensor]
    Gateway --> Actuator1[Motor Controller]
    Gateway --> Actuator2[LED Controller]
    
    CloudAPI --> Database[(Database)]
    Database --> Analytics[Analytics Engine]
```

## Example 4: MVC Pattern

```mermaid
graph TD
    User[User] --> View[View Layer]
    View --> Controller[Controller]
    Controller --> Model[Model]
    Model --> DB[(Database)]
    Model --> Controller
    Controller --> View
    View --> User
```

## Example 5: Event-Driven Architecture

```mermaid
graph LR
    Source1[Event Source 1] --> EventBus[Event Bus]
    Source2[Event Source 2] --> EventBus
    Source3[Event Source 3] --> EventBus
    
    EventBus --> Handler1[Event Handler 1]
    EventBus --> Handler2[Event Handler 2]
    EventBus --> Handler3[Event Handler 3]
    
    Handler1 --> Action1[Action/Service 1]
    Handler2 --> Action2[Action/Service 2]
    Handler3 --> Action3[Action/Service 3]
```

## Example 6: State Machine

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Processing: Start
    Processing --> Success: Complete
    Processing --> Error: Fail
    Success --> [*]
    Error --> Retry: Retry
    Retry --> Processing
    Error --> [*]: Give Up
```

## Example 7: Sequence Diagram

```mermaid
sequenceDiagram
    participant User
    participant Frontend
    participant Backend
    participant Database
    
    User->>Frontend: Request Data
    Frontend->>Backend: API Call
    Backend->>Database: Query
    Database-->>Backend: Results
    Backend-->>Frontend: Response
    Frontend-->>User: Display Data
```

## How to Use

1. Copy the example that best matches your needs
2. Modify the component names to match your actual system
3. Add or remove connections as needed
4. Update the `component-diagram.md` file with your customized diagram

## Tips for Mermaid Diagrams

- Use `graph TD` for top-down diagrams
- Use `graph LR` for left-right diagrams
- Use `-->` for directional arrows
- Use `---` for non-directional connections
- Use square brackets `[]` for rectangular nodes
- Use parentheses `()` for rounded nodes
- Use cylinder notation `[()]` for databases
- Add colors with `style NodeName fill:#color`
