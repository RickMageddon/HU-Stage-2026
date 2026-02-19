```mermaid
graph LR
    
 
    %% Hoofdcomponenten
    Lipo[Lipo Accu]:::battery
    PSU[PSU]:::power
    
    subgraph Control [Besturing & Servo's]
        Driver[Esp32 Servo Driver]:::signal
        S1[Servo 1]
        S2[Servo 2]
        S3[Servo 3]
        S4[Servo 4]
    end
    

    subgraph Drive [Aandrijving]
        Rx[Receiver] --- ESC1[Esc 1]
        Rx[Receiver] --- ESC2[Esc 2]
        Rx[Receiver] --- ESC3[Esc 3]
        Rx[Receiver] --- ESC4[Esc 4]
        
        subgraph ESC_Motor_1
            ESC1[Esc 1] --- M1[Linear Actuator 1]
        end
        subgraph ESC_Motor_2
            ESC2[Esc 2] --- M2[Linear Actuator 2]
        end
        subgraph ESC_Motor_3
            ESC3[Esc 3] --- M3[Linear Actuator 3]
        end
        subgraph ESC_Motor_4
            ESC4[Esc 4] --- M4[Linear Actuator 4]
        end
    end

    %% --- VERBINDINGEN ---

    %% Voeding (Power Flow) - Rood/Zwart in origineel
    Lipo ==> PSU
    PSU ==> Driver
    PSU ==> ESC1
    PSU ==> ESC2
    PSU ==> ESC3
    PSU ==> ESC4

    %% Servo Signaal & Power intern
    Driver --> S1
    Driver --> S2
    Driver --> S3
    Driver --> S4

