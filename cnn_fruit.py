import cv2
import urllib.request
import numpy as np
import requests
import threading

# Địa chỉ IP của ESP32
esp32_ip = "http://192.168.1.3"

# Định nghĩa góc servo cho từng màu
angle_map = {
    "green": 30,
    "blue": 60,
    "red": 90,
    "black": 120,
    "yellow": 150  # Thêm màu vàng
}

# URL camera
url = 'http://192.168.1.4/cam-lo.jpg'
cv2.namedWindow("live transmission", cv2.WINDOW_AUTOSIZE)

# Biến toàn cục để lưu frame mới nhất
frame = None
lock = threading.Lock()


def fetch_frame():
    global frame
    while True:
        try:
            img_resp = urllib.request.urlopen(url, timeout=1)
            imgnp = np.array(bytearray(img_resp.read()), dtype=np.uint8)
            new_frame = cv2.imdecode(imgnp, -1)
            new_frame = cv2.resize(new_frame, (640, 480))
            with lock:
                frame = new_frame
        except:
            print("🔴 Lỗi khi lấy ảnh từ camera!")


# Chạy luồng riêng để lấy ảnh từ camera
thread = threading.Thread(target=fetch_frame, daemon=True)
thread.start()

while True:
    with lock:
        if frame is None:
            continue
        current_frame = frame.copy()

    hsv = cv2.cvtColor(current_frame, cv2.COLOR_BGR2HSV)
    hsv = cv2.GaussianBlur(hsv, (5, 5), 0)

    # Định nghĩa phạm vi màu HSV
    color_ranges = {
        "blue": ([92, 57, 50], [142, 153, 178]),
        "red1": ([0, 120, 70], [10, 255, 255]),
        "red2": ([170, 120, 70], [180, 255, 255]),
        "green": ([35, 100, 50], [85, 255, 255]),
        "black": ([0, 0, 0], [180, 255, 30]),
        "yellow": ([20, 100, 100], [30, 255, 255])  # Thêm màu vàng
    }

    # Tạo mask
    color_masks = {}
    for color, (lower, upper) in color_ranges.items():
        lower_b = np.array(lower, dtype=np.uint8)
        upper_b = np.array(upper, dtype=np.uint8)
        mask = cv2.inRange(hsv, lower_b, upper_b)

        if color == "red2":
            if "red" in color_masks:
                color_masks["red"] += mask
            else:
                color_masks["red"] = mask
        else:
            color_masks[color] = mask

    # Tìm màu có diện tích lớn nhất
    max_area = 0
    dominant_color = "Unknown"
    dominant_contour = None

    for color, mask in color_masks.items():
        cnts, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)
        for c in cnts:
            area = cv2.contourArea(c)
            if area > max_area and area > 2000:
                max_area = area
                dominant_color = color
                dominant_contour = c

    # Vẽ màu có xác suất cao nhất
    if dominant_contour is not None:
        cv2.drawContours(current_frame, [dominant_contour], -1, (0, 255, 255), 3)
        M = cv2.moments(dominant_contour)
        if M["m00"] != 0:
            cx = int(M["m10"] / M["m00"])
            cy = int(M["m01"] / M["m00"])
            cv2.circle(current_frame, (cx, cy), 7, (255, 255, 255), -1)
            cv2.putText(current_frame, dominant_color, (cx - 20, cy - 20), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

            # Gửi góc servo đến ESP32
            angle = angle_map.get(dominant_color, 0)
            try:
                requests.get(f"{esp32_ip}/setServo?angle={angle}")
                print(f"🟢 Đã gửi {angle}° đến ESP32")
            except:
                print("🔴 Lỗi khi gửi lệnh đến ESP32")

    # Hiển thị ảnh
    cv2.imshow("live transmission", current_frame)

    # Nhấn 'q' để thoát
    key = cv2.waitKey(1)
    if key == ord('q'):
        break

cv2.destroyAllWindows()