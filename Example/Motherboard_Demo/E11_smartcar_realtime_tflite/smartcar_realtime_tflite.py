import cv2
import numpy as np
from pathlib import Path
from typing import Optional, Tuple, List
from dataclasses import dataclass, field

import tensorflow as tf


SMARTCAR_CLASSES = ["交通工具-直行", "武器-左", "物资-右"]


@dataclass
class A4DetectionResult:
    success: bool
    tl: Optional[np.ndarray] = field(default=None)
    tr: Optional[np.ndarray] = field(default=None)
    br: Optional[np.ndarray] = field(default=None)
    bl: Optional[np.ndarray] = field(default=None)
    pt_top_left: Optional[Tuple[int, int]] = field(default=None)
    pt_top_right: Optional[Tuple[int, int]] = field(default=None)
    contour: Optional[np.ndarray] = field(default=None)
    message: Optional[str] = field(default=None)


def create_red_mask(
    hsv: np.ndarray,
    lower_red1: np.ndarray,
    upper_red1: np.ndarray,
    lower_red2: np.ndarray,
    upper_red2: np.ndarray,
) -> np.ndarray:
    mask1 = cv2.inRange(hsv, lower_red1, upper_red1)
    mask2 = cv2.inRange(hsv, lower_red2, upper_red2)
    return cv2.bitwise_or(mask1, mask2)


def apply_morphology(mask: np.ndarray, kernel_size: int = 3) -> np.ndarray:
    kernel = np.ones((kernel_size, kernel_size), np.uint8)
    mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)
    mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
    return mask


def filter_center_contours(
    contours: List,
    img_width: int,
    center_ratio: float = 0.5,
    min_area: float = 1000,
    min_width: int = 40,
    min_height: int = 20,
) -> List:
    center_range = (
        (0.5 - center_ratio / 2) * img_width,
        (0.5 + center_ratio / 2) * img_width,
    )
    filtered = []
    for cnt in contours:
        area = cv2.contourArea(cnt)
        x, y, cw, ch = cv2.boundingRect(cnt)
        cnt_center_x = x + cw / 2
        if (
            center_range[0] < cnt_center_x < center_range[1]
            and area > min_area
            and cw > min_width
            and ch > min_height
        ):
            filtered.append((cnt, area, x, y, cw, ch))
    return sorted(filtered, key=lambda r: r[1], reverse=True)


def find_corner_points(
    points: np.ndarray,
) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
    add = points.sum(axis=1)
    diff = np.diff(points, axis=1)
    tl = points[np.argmin(add)]
    br = points[np.argmax(add)]
    tr = points[np.argmin(diff)]
    bl = points[np.argmax(diff)]
    return tl, tr, br, bl


def compute_top_edge_points(
    src_pts: np.ndarray,
    phys_width: float,
    phys_img_height: float,
    phys_red_height: float,
) -> Tuple[np.ndarray, np.ndarray]:
    w, h_img, h_red = phys_width, phys_img_height, phys_red_height
    dst_pts = np.array(
        [[0, h_img], [w, h_img], [w, h_img + h_red], [0, h_img + h_red]],
        dtype=np.float32,
    )
    M = cv2.getPerspectiveTransform(dst_pts, src_pts)
    top_points_physical = np.array([[[0, 0], [w, 0]]], dtype=np.float32)
    top_points_image = cv2.perspectiveTransform(top_points_physical, M)[0]
    return tuple(top_points_image[0].astype(int)), tuple(
        top_points_image[1].astype(int)
    )


def perspective_crop(
    img: np.ndarray,
    tl: np.ndarray,
    tr: np.ndarray,
    pt_top_left: Optional[Tuple[int, int]],
    pt_top_right: Optional[Tuple[int, int]],
) -> np.ndarray:
    src_quad = np.array([tl, tr, pt_top_right, pt_top_left], dtype=np.float32)
    width = int(np.linalg.norm(tr - tl))
    height = int(np.linalg.norm(pt_top_left - tl))
    dst_quad = np.array(
        [[0, 0], [width, 0], [width, height], [0, height]], dtype=np.float32
    )
    M_warp = cv2.getPerspectiveTransform(src_quad, dst_quad)
    return cv2.warpPerspective(img, M_warp, (width, height))


def detect_a4_points(
    img: np.ndarray,
    red_hsv_ranges: Optional[
        Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]
    ] = None,
    morphology_kernel_size: int = 3,
    center_ratio: float = 0.5,
    min_area: float = 1000,
    min_width: int = 40,
    min_height: int = 20,
    phys_width: float = 12.0,
    phys_img_height: float = 12.0,
    phys_red_height: float = 5.0,
) -> A4DetectionResult:
    h, w = img.shape[:2]
    hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

    if red_hsv_ranges is None:
        lower_red1 = np.array([0, 100, 100])
        upper_red1 = np.array([15, 255, 255])
        lower_red2 = np.array([150, 100, 100])
        upper_red2 = np.array([180, 255, 255])
    else:
        lower_red1, upper_red1, lower_red2, upper_red2 = red_hsv_ranges

    mask = create_red_mask(hsv, lower_red1, upper_red1, lower_red2, upper_red2)
    mask = apply_morphology(mask, morphology_kernel_size)
    contours, _ = cv2.findContours(mask, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)

    if not contours:
        return A4DetectionResult(success=False, message="No red region found")

    center_contours = filter_center_contours(
        contours, w, center_ratio, min_area, min_width, min_height
    )

    if not center_contours:
        return A4DetectionResult(success=False, message="No center red region found")

    best_cnt = center_contours[0][0]
    points = best_cnt.reshape(-1, 2)
    tl, tr, br, bl = find_corner_points(points)

    src_pts = np.array([tl, tr, br, bl], dtype=np.float32)
    pt_top_left, pt_top_right = compute_top_edge_points(
        src_pts, phys_width, phys_img_height, phys_red_height
    )

    return A4DetectionResult(
        success=True,
        tl=tl,
        tr=tr,
        br=br,
        bl=bl,
        pt_top_left=pt_top_left,
        pt_top_right=pt_top_right,
        contour=best_cnt,
    )


def draw_contour(
    result: np.ndarray,
    contour: np.ndarray,
    color: Tuple[int, int, int] = (255, 0, 0),
    thickness: int = 1,
) -> None:
    cv2.drawContours(result, [contour], -1, color, thickness)


def draw_corner_points(
    result: np.ndarray,
    corners: List[np.ndarray],
    color: Tuple[int, int, int] = (0, 255, 0),
    radius: int = 1,
    thickness: int = -1,
) -> None:
    for point in corners:
        px, py = point.ravel()
        cv2.circle(result, (px, py), radius, color, thickness)


def extend_line(
    point1: np.ndarray,
    point2: np.ndarray,
    extend_length: float,
) -> Tuple[Tuple[int, int], Tuple[int, int]]:
    mid_x = (point1[0] + point2[0]) / 2
    mid_y = (point1[1] + point2[1]) / 2
    dir_x = point1[0] - point2[0]
    dir_y = point1[1] - point2[1]
    length = np.sqrt(dir_x**2 + dir_y**2)
    dir_x /= length
    dir_y /= length
    pt1 = (int(mid_x - dir_x * extend_length), int(mid_y - dir_y * extend_length))
    pt2 = (int(mid_x + dir_x * extend_length), int(mid_y + dir_y * extend_length))
    return pt1, pt2


def draw_extended_lines(
    result: np.ndarray,
    tl: np.ndarray,
    bl: np.ndarray,
    tr: np.ndarray,
    br: np.ndarray,
    extend_length: float = 500,
    color: Tuple[int, int, int] = (0, 255, 0),
    thickness: int = 1,
) -> None:
    left_pt1, left_pt2 = extend_line(tl, bl, extend_length)
    right_pt1, right_pt2 = extend_line(tr, br, extend_length)
    cv2.line(result, left_pt1, left_pt2, color, thickness)
    cv2.line(result, right_pt1, right_pt2, color, thickness)


def draw_top_edge(
    result: np.ndarray,
    tl: np.ndarray,
    tr: np.ndarray,
    pt_top_left: Optional[Tuple[int, int]],
    pt_top_right: Optional[Tuple[int, int]],
    line_color: Tuple[int, int, int] = (0, 0, 255),
    corner_color: Tuple[int, int, int] = (0, 0, 255),
    thickness: int = 1,
    corner_radius: int = 2,
) -> None:
    cv2.line(result, pt_top_left, pt_top_right, line_color, thickness)
    cv2.circle(result, pt_top_left, corner_radius, corner_color, -1)
    cv2.circle(result, pt_top_right, corner_radius, corner_color, -1)
    cv2.line(result, tuple(tl.astype(int)), pt_top_left, (0, 255, 0), thickness)
    cv2.line(result, tuple(tr.astype(int)), pt_top_right, (0, 255, 0), thickness)


def visualize_detection(
    img: np.ndarray,
    detection: A4DetectionResult,
    extend_length: float = 500.0,
) -> np.ndarray:
    if not detection.success:
        return img.copy()

    result = img.copy()
    draw_contour(result, detection.contour)
    draw_corner_points(result, [detection.tl, detection.tr, detection.br, detection.bl])
    draw_extended_lines(
        result,
        detection.tl,
        detection.bl,
        detection.tr,
        detection.br,
        extend_length,
    )
    draw_top_edge(
        result,
        detection.tl,
        detection.tr,
        detection.pt_top_left,
        detection.pt_top_right,
    )
    return result


def preprocess_for_model(warped: np.ndarray, img_size: int = 96) -> np.ndarray:
    resized = cv2.resize(warped, (img_size, img_size))
    normalized = (resized.astype(np.float32) / 127.5) - 1.0
    return np.expand_dims(normalized, axis=0)


def main():
    interpreter = tf.lite.Interpreter(model_path="smartcar_model.tflite")
    interpreter.allocate_tensors()
    input_details = interpreter.get_input_details()
    output_details = interpreter.get_output_details()

    output_index = output_details[0]["index"]

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("Cannot open camera")
        return

    print("Press 'q' to quit")

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        detection = detect_a4_points(frame)
        annotated = visualize_detection(frame, detection)
        if detection.success:
            warped = perspective_crop(
                frame,
                detection.tl,
                detection.tr,
                detection.pt_top_left,
                detection.pt_top_right,
            )
            warped = cv2.rotate(warped, cv2.ROTATE_180)

            model_input = preprocess_for_model(warped)
            interpreter.set_tensor(input_details[0]["index"], model_input)
            interpreter.invoke()

            output = interpreter.get_tensor(output_index)[0]
            probs = tf.nn.softmax(output).numpy()
            pred_idx = int(np.argmax(probs))
            pred_cls = SMARTCAR_CLASSES[pred_idx]
            confidence = probs[pred_idx]

            cv2.putText(
                annotated,
                f"{pred_cls}: {confidence:.4f}",
                (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                1,
                (0, 255, 0),
                2,
            )

            print(f"\r{pred_cls}: {confidence:.4f}   ", end="", flush=True)
        else:
            print(f"\rNo detection         ", end="", flush=True)

        cv2.imshow("SmartCar Detection", annotated)

        if cv2.waitKey(1) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyAllWindows()
    print()


if __name__ == "__main__":
    main()
