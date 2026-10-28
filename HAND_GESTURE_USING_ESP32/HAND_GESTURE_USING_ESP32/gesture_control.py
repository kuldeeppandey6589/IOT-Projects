import cv2
import mediapipe as mp
import requests

ESP32_IP = "10.229.174.181"

mp_hands = mp.solutions.hands
hands = mp_hands.Hands()
mp_draw = mp.solutions.drawing_utils

cap = cv2.VideoCapture(2)

prev_command = ""

def count_fingers(hand_landmarks):
    finger_tips = [4, 8, 12, 16, 20]
    fingers = []

    # Thumb
    if hand_landmarks.landmark[finger_tips[0]].x < hand_landmarks.landmark[finger_tips[0] - 1].x:
        fingers.append(1)
    else:
        fingers.append(0)

    # Other fingers
    for tip in finger_tips[1:]:
        if hand_landmarks.landmark[tip].y < hand_landmarks.landmark[tip - 2].y:
            fingers.append(1)
        else:
            fingers.append(0)

    return fingers.count(1)

while True:
    success, img = cap.read()

    img_rgb = cv2.cvtColor(img, cv2.COLOR_BGR2RGB)
    result = hands.process(img_rgb)

    command = ""

    if result.multi_hand_landmarks:
        for handLms in result.multi_hand_landmarks:

            mp_draw.draw_landmarks(
                img,
                handLms,
                mp_hands.HAND_CONNECTIONS
            )

            total_fingers = count_fingers(handLms)

            cv2.putText(
                img,
                f'Fingers: {total_fingers}',
                (10, 70),
                cv2.FONT_HERSHEY_SIMPLEX,
                1,
                (255, 0, 0),
                2
            )

            try:
                if total_fingers == 1:
                    command = "lighton"

                elif total_fingers == 2:
                    command = "lightoff"

                elif total_fingers == 3:
                    command = "fanon"

                elif total_fingers == 4:
                    command = "fanoff"

                elif total_fingers == 5:
                    command = "alloff"

                if command != "" and command != prev_command:
                    url = f"http://{ESP32_IP}/{command}"

                    response = requests.get(url)

                    print("Command Sent:", command)

                    prev_command = command

            except Exception as e:
                print("Error:", e)

    cv2.imshow("Hand Gesture Home Automation", img)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
cv2.destroyAllWindows()

