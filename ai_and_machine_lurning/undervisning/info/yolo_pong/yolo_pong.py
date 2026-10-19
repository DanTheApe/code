"""Webcam Pong controlled by YOLO26 human wrist positions.
-- (with using free model weights yolo26n-pose.pt (~7.5MB), from https://github.com/ultralytics/ultralytics or https://huggingface.co/f416720001/YOLO26 )

Run: python yolo_pong.py
Controls: 
"1" / "2":  choose a mode, single player or two playerson the start menu 
"Q": quit  
"R": restart |
"P": pause/resume 
"M": back to the menu.

Modes:
  1 Single player: one person controls both paddles, left hand = left
    (blue) paddle, right hand = right (green) paddle.
  2 Two players: stand side by side. The left player uses their left
    hand for the left paddle; the right player uses their right hand
    for the right paddle.

Set up:
    Use Python 3.10+
    Put the `yolo_pong.py` and `requirements.txt` files in one folder.

    Windows / Mac / Linux terminal run
        ```
        python -m pip install --upgrade pip
        python -m pip install -r requirements.txt
        python yolo_pong.py
    ```
    NOTE: The first run requires an internet connection to download `yolo26n-pose.pt` weights.

    
Citation &  Ackonwledgements:
* This Python code was developed with the assistance of Claude code. 
* @misc{jocher2026ultralyticsyolo26unifiedrealtime,
  title = {Ultralytics YOLO26: Unified Real-Time End-to-End Vision Models},
  author = {Glenn Jocher and Jing Qiu and Mengyu Liu and Shuai Lyu and Fatih Cagatay Akyon and Muhammet Esat Kalfaoglu},
  year = {2026},
  eprint = {2606.03748},
  archivePrefix = {arXiv},
  primaryClass = {cs.CV},
  doi = {10.48550/arXiv.2606.03748},
  url = {https://arxiv.org/abs/2606.03748},
}
"""

import random
import time

import cv2
import numpy as np


MODEL_NAME = "yolo26n-pose.pt"
CAMERA_INDEX = 0  # change to 1 if the default webcam is not the one you want to use
CAMERA_WIDTH, CAMERA_HEIGHT = 640, 480
INFERENCE_SIZE = 416     # reduce if you have low performance CPU, for example using 320 instead.
MIN_WRIST_CONFIDENCE = 0.40

WINDOW_NAME = "YOLO GESTURE PONG"
GAME_WIDTH, GAME_HEIGHT = 800, 500
PREVIEW_WIDTH = 400
TOP, BOTTOM = 56, GAME_HEIGHT - 12
PADDLE_HEIGHT, PADDLE_WIDTH = 85, 14
LEFT_X, RIGHT_X = 32, GAME_WIDTH - 32 - PADDLE_WIDTH
BALL_RADIUS = 10
MATCH_ROUNDS = 11       # Best of 11: keep this odd so no tie.
WINNING_SCORE = MATCH_ROUNDS // 2 + 1   # 6 wins, by round 11 .
SERVE_DELAY = 1.0

BALL_SPEED = 325.0      # Starting horizontal ball speed, pixels per second.
MAX_BALL_SPEED = 900.0
# Every SPEED_UP_INTERVAL seconds of play, the ball gets SPEED_UP_STEP times faster.
SPEED_UP_INTERVAL = 30.0  #second
SPEED_UP_STEP = 1.25  # 25% increase each time, up to MAX_BALL_SPEED.

# Wrist pixel positions rarely span the full camera frame (a raised hand
# doesn't reach y=0, a lowered one doesn't reach the bottom edge), so the
# paddle would see issue to reach TOP/BOTTOM if mapped off frame_height
# directly. Instead we track each hand's actual reachable range live.
WRIST_RANGE_SEED_FRACTION = 0.35     # initial guessed range, as a fraction of frame height
WRIST_RANGE_MIN_FRACTION = 0.30      # floor on the range so small jitter isn't twitchy
WRIST_RANGE_EXPAND_RATE = 0.3       # per second; how fast the range grows to a new extreme
# Per second; how fast the range re-tightens if unused. 0 = never shrink during
# play, so a still hand's paddle doesn't drift.
WRIST_RANGE_CONTRACT_RATE = 0.0
# Fraction of the learned range at each end that already maps to the paddle's
# extreme. The range never shrinks, so one unusually low (or high) reading
# would otherwise leave the paddle unable to reach BOTTOM (or TOP) afterwards.
WRIST_RANGE_EDGE_MARGIN = 0.25

# One Euro filter on the wrist y: strong smoothing when the hand is nearly
# still, little lag when it moves fast.
# Lower MIN_CUTOFF = steadier but laggier
# higher BETA = snappier fast moves
FILTER_MIN_CUTOFF = 0.5     # Hz
FILTER_BETA = 0.01
FILTER_D_CUTOFF = 1.0       # Hz
# Keep a hand's last position this long when its wrist briefly drops out.
WRIST_HOLD_SECONDS = 0.25
# Wrists closer than this (x pixels, PX) are ambiguous, so they keep the side
# they were on last frame instead of being re-sorted left/right.
WRIST_SWAP_GUARD_PX = 80

BLUE = (255, 180, 40)   # OpenCV colors are BGR.
GREEN = (65, 230, 100)
WHITE = (245, 245, 245)
GRAY = (105, 105, 105)
YELLOW = (0, 220, 255)

LEFT, RIGHT = 0, 1  # Indexes into Pong.scores / Pong.paddles.

# Game modes, chosen on the start menu.
SINGLE_PLAYER, TWO_PLAYER = "single", "two"
SIDE_NAMES = {
    SINGLE_PLAYER: ("LEFT HAND", "RIGHT HAND"),
    TWO_PLAYER: ("PLAYER 1", "PLAYER 2"),
}
LEFT_WRIST, RIGHT_WRIST = 9, 10  # COCO human-pose keypoint indexes. see: https://docs.ultralytics.com/tasks/pose in the section of "Keypoint Index map"


def clamp(value, low, high):
    return max(low, min(high, value))


def select_player(boxes):
    """Return the detection index of the player (the biggest box), or None.
    """
    if len(boxes) == 0:
        return None
    return max(
        range(len(boxes)),
        key=lambda i: (boxes[i][2] - boxes[i][0])
        
        * (boxes[i][3] - boxes[i][1]),
    )


def _distance(a, b):
    return float(np.hypot(a[0] - b[0], a[1] - b[1]))


def split_wrists(points, body_center_x, previous=(None, None)):
    """Assign wrist (x, y) points to [left paddle, right paddle].
    
    
    

    The frame is mirrored, so the player's right hand appears on the right
    of the image. Pose models label left/right by body appearance, which a
    mirrored image can swap, so we use screen position instead of labels.

    `previous` holds each side's last known wrist (or None). When the
    assignment is ambiguous (two wrists close together in x, or only one
    wrist visible), each wrist keeps the side it was nearest to last frame,
    so paddles don't swap. Otherwise wrists are sorted left to right, or a
    single wrist goes to the side of the body it is on.
    """
    sides = [None, None]
    points = sorted(points)
    both_known = previous[LEFT] is not None and previous[RIGHT] is not None
    if len(points) >= 2:
        a, b = points[0], points[-1]
        if both_known and b[0] - a[0] < WRIST_SWAP_GUARD_PX:
            kept = _distance(a, previous[LEFT]) + _distance(b, previous[RIGHT])
            swapped = _distance(a, previous[RIGHT]) + _distance(b, previous[LEFT])
            if swapped < kept:
                a, b = b, a
        sides[LEFT], sides[RIGHT] = a, b
    elif len(points) == 1:
        point = points[0]
        if both_known:
            side = min((LEFT, RIGHT), key=lambda s: _distance(point, previous[s]))
        else:
            side = LEFT if point[0] < body_center_x else RIGHT
        sides[side] = point
    return sides


class OneEuroFilter:
    """Speed-adaptive low-pass filter (Casiez et al., 2012).

    The cutoff frequency rises with the signal's speed: slow movement is
    smoothed heavily, fast movement passes through with little lag. Using
    the real time step makes it behave the same at any frame rate.
    """

    def __init__(self, min_cutoff=FILTER_MIN_CUTOFF, beta=FILTER_BETA,
                 d_cutoff=FILTER_D_CUTOFF):
        self.min_cutoff, self.beta, self.d_cutoff = min_cutoff, beta, d_cutoff
        self.reset()

    def reset(self):
        self.value = None
        self.speed = 0.0

    @staticmethod
    def _alpha(cutoff, dt):
        tau = 1.0 / (2 * np.pi * cutoff)
        return 1.0 / (1.0 + tau / dt)

    def __call__(self, x, dt):
        if self.value is None:
            self.value = float(x)
            return self.value
        if dt <= 0:
            return self.value
        raw_speed = (x - self.value) / dt
        self.speed += self._alpha(self.d_cutoff, dt) * (raw_speed - self.speed)
        cutoff = self.min_cutoff + self.beta * abs(self.speed)
        self.value += self._alpha(cutoff, dt) * (x - self.value)
        return self.value


def wrist_to_paddle(wrist_y, frame_height, y_range=None):
    """Map a camera y coordinate to the paddle's *top* coordinate.

    By default this spans the full frame height. Pass y_range=(low, high)
    to map a hand's actually-observed reach instead, so the paddle can
    reach TOP/BOTTOM without requiring the wrist at the camera's edges.
    """
    if y_range is None:
        y_low, y_high = 0.0, frame_height
    else:
        # Shrink the learned range inward so the paddle hits TOP/BOTTOM a bit
        # before the hand reaches its recorded extremes.
        y_low, y_high = y_range
        margin = (y_high - y_low) * WRIST_RANGE_EDGE_MARGIN
        y_low, y_high = y_low + margin, y_high - margin
    span = max(y_high - y_low, 1.0)
    fraction = clamp((float(wrist_y) - y_low) / span, 0.0, 1.0)
    target_center = TOP + fraction * (BOTTOM - TOP)
    return clamp(
        target_center - PADDLE_HEIGHT / 2,
        TOP,
        BOTTOM - PADDLE_HEIGHT,
    )


class WristRangeTracker:
    """Adaptively learns one hand's reachable vertical wrist range.

    Expands quickly toward a newly reached extreme so full paddle travel
    becomes available as soon as the player swings that far; contracts
    slowly when unused so the game re-tightens (stays sensitive) if the
    player settles into a smaller range, without needing a calibration step.
    """

    def __init__(self, frame_height):
        mid = frame_height / 2
        self.min_span = frame_height * WRIST_RANGE_MIN_FRACTION
        seed_span = frame_height * WRIST_RANGE_SEED_FRACTION
        self.min_y = mid - seed_span / 2
        self.max_y = mid + seed_span / 2

    def update(self, y, dt):
        rate = WRIST_RANGE_EXPAND_RATE if y < self.min_y else WRIST_RANGE_CONTRACT_RATE
        self.min_y += (y - self.min_y) * min(1.0, rate * dt)
        rate = WRIST_RANGE_EXPAND_RATE if y > self.max_y else WRIST_RANGE_CONTRACT_RATE
        self.max_y += (y - self.max_y) * min(1.0, rate * dt)

        if self.max_y - self.min_y < self.min_span:
            center = (self.max_y + self.min_y) / 2
            self.min_y = center - self.min_span / 2
            self.max_y = center + self.min_span / 2

    @property
    def range(self):
        return (self.min_y, self.max_y)


class HandTracker:
    """One hand's state: smoothed wrist y, learned reach, and dropout hold."""

    def __init__(self, frame_height):
        self.frame_height = frame_height
        self.reset()

    def reset(self):
        self.reach = WristRangeTracker(self.frame_height)
        self.filter = OneEuroFilter()
        self.last_point = None      # last raw (x, y) wrist, for side matching
        self.last_seen = None
        self.smoothed_y = None

    def is_live(self, now):
        """True if seen recently enough to keep using the last position."""
        return self.last_seen is not None and now - self.last_seen <= WRIST_HOLD_SECONDS

    def observe(self, point, now):
        """Feed a new raw wrist (x, y); returns the paddle top it maps to."""
        if not self.is_live(now):
            self.filter.reset()  # After a long gap, don't glide from a stale spot.
        dt = 0.0 if self.last_seen is None else now - self.last_seen
        self.smoothed_y = self.filter(point[1], dt)
        self.reach.update(self.smoothed_y, dt)
        self.last_point, self.last_seen = point, now
        return self.paddle_top()

    def paddle_top(self):
        return wrist_to_paddle(self.smoothed_y, self.frame_height, self.reach.range)


def assign_players(boxes, frame_width):
    """Two-player mode: map detection indexes to camera-left P1 and camera-right P2.

    When 3+ people appear, keep the two biggest boxes. For one person,
    use their frame half; two detected people are ordered by x center.
    This is a spatial assignment, NOT persistent identity tracking.
    """
    if len(boxes) == 0:
        return [None, None]

    ranked = sorted(
        range(len(boxes)),
        key=lambda i: (boxes[i][2] - boxes[i][0])
        * (boxes[i][3] - boxes[i][1]),
        reverse=True,
    )[:2]
    ranked.sort(key=lambda i: (boxes[i][0] + boxes[i][2]) / 2)

    if len(ranked) == 2:
        return [ranked[0], ranked[1]]

    i = ranked[0]
    center_x = (boxes[i][0] + boxes[i][2]) / 2
    if center_x < frame_width / 2:
        return [i, None]
    return [None, i]


def pick_outer_wrist(points, body_center_x, side):
    """Two-player mode: pick the wrist on the outer side of a player's body.

    P1 (left) plays with their left hand, which the mirrored view shows on
    the left of their body; P2 (right) uses their right hand, shown on the
    right. Wrists on the inner side are ignored so the wrong hand can't
    take over when the correct one is briefly lost.
    """
    if side == LEFT:
        outer = [p for p in points if p[0] <= body_center_x]
        return min(outer) if outer else None
    outer = [p for p in points if p[0] >= body_center_x]
    return max(outer) if outer else None


def _pose_arrays(result):
    """Return (boxes, keypoint xy, keypoint confidences or None), or None if no people."""
    if result.boxes is None or result.keypoints is None:
        return None
    boxes = result.boxes.xyxy.cpu().numpy()
    xy = result.keypoints.xy.cpu().numpy()
    if len(boxes) == 0 or len(xy) == 0:
        return None
    conf_tensor = result.keypoints.conf
    confidences = conf_tensor.cpu().numpy() if conf_tensor is not None else None
    return boxes, xy, confidences


def _person_wrists(xy, confidences, index):
    """Confident wrist (x, y) points of one detected person."""
    if index is None or index >= len(xy) or len(xy[index]) <= RIGHT_WRIST:
        return []
    points = []
    for keypoint in (LEFT_WRIST, RIGHT_WRIST):
        if (confidences is not None
                and confidences[index][keypoint] < MIN_WRIST_CONFIDENCE):
            continue
        x, y = xy[index][keypoint]
        if not (np.isfinite(x) and np.isfinite(y)):
            continue
        if x <= 0 or y <= 0:
            continue
        points.append((int(x), int(y)))
    return points


def _body_center_x(box):
    return (box[0] + box[2]) / 2


def detect_player_wrists(result):
    """Single-player mode: return (wrist points, body center x, player box index).

    The points are unassigned; pass them to split_wrists() to pick sides.
    """
    arrays = _pose_arrays(result)
    if arrays is None:
        return [], None, None
    boxes, xy, confidences = arrays
    index = select_player(boxes)
    if index is None:
        return [], None, None
    return _person_wrists(xy, confidences, index), _body_center_x(boxes[index]), index


def detect_two_player_wrists(result, frame_width):
    """Two-player mode: return ([P1 wrist or None, P2 wrist or None], [P1 index, P2 index])."""
    arrays = _pose_arrays(result)
    if arrays is None:
        return [None, None], [None, None]
    boxes, xy, confidences = arrays
    indexes = assign_players(boxes, frame_width)
    wrists = [None, None]
    for side, index in enumerate(indexes):
        if index is not None:
            points = _person_wrists(xy, confidences, index)
            wrists[side] = pick_outer_wrist(points, _body_center_x(boxes[index]), side)
    return wrists, indexes


class Pong:
    def __init__(self):
        self.restart()

    def restart(self):
        self.scores = [0, 0]
        self.paddles = [
            (TOP + BOTTOM - PADDLE_HEIGHT) / 2,
            (TOP + BOTTOM - PADDLE_HEIGHT) / 2,
        ]
        self.winner = None
        self.ball_x, self.ball_y = GAME_WIDTH / 2, (TOP + BOTTOM) / 2
        self.ball_vx = self.ball_vy = 0.0
        self.serve_direction = random.choice([-1, 1])
        self.serve_at = None
        self.play_time = 0.0  # Seconds the ball has been in motion this match.

    def schedule_serve(self, now, direction):
        self.serve_direction = direction
        self.serve_at = now + SERVE_DELAY
        self.ball_x, self.ball_y = GAME_WIDTH / 2, (TOP + BOTTOM) / 2
        self.ball_vx = self.ball_vy = 0.0

    def start_serve(self):
        scale = self.ball_speed / BALL_SPEED
        self.ball_vx = self.ball_speed * self.serve_direction
        self.ball_vy = random.choice([-1, 1]) * random.uniform(90, 165) * scale
        self.serve_at = None

    @property
    def rounds_played(self):
        return sum(self.scores)

    @property
    def speed_level(self):
        """How many speed-ups have happened (0 during the first interval)."""
        return int(self.play_time // SPEED_UP_INTERVAL)

    @property
    def ball_speed(self):
        """Current horizontal ball speed for this point in the match."""
        return min(MAX_BALL_SPEED, BALL_SPEED * SPEED_UP_STEP ** self.speed_level)

    @property
    def seconds_to_speed_up(self):
        return SPEED_UP_INTERVAL - self.play_time % SPEED_UP_INTERVAL

    def point_for(self, scorer, now):
        self.scores[scorer] += 1
        self.ball_vx = self.ball_vy = 0.0
        if self.scores[scorer] >= WINNING_SCORE:
            self.winner = scorer
            self.serve_at = None
        else:
            # Serve towards the side that missed.
            self.schedule_serve(now, direction=1 if scorer == LEFT else -1)

    def update(self, dt, now):
        """Update only when both wrists are visible and not manually paused."""
        if self.winner is not None:
            return
        if self.serve_at is None and self.ball_vx == 0:
            self.schedule_serve(now, self.serve_direction)
            return
        if self.serve_at is not None:
            if now >= self.serve_at:
                self.start_serve()
            return

        dt = clamp(dt, 0.0, 0.05)
        # Timed speed-up: when the play clock crosses the next interval,
        # scale the moving ball up right away (keeping its direction).
        old_speed = self.ball_speed
        self.play_time += dt
        if self.ball_speed != old_speed:
            factor = self.ball_speed / old_speed
            self.ball_vx *= factor
            self.ball_vy *= factor

        old_x, old_y = self.ball_x, self.ball_y
        self.ball_x += self.ball_vx * dt
        self.ball_y += self.ball_vy * dt

        # Bounce against arena ceiling and floor.
        if self.ball_y - BALL_RADIUS < TOP:
            self.ball_y = TOP + BALL_RADIUS
            self.ball_vy = abs(self.ball_vy)
        if self.ball_y + BALL_RADIUS > BOTTOM:
            self.ball_y = BOTTOM - BALL_RADIUS
            self.ball_vy = -abs(self.ball_vy)

        # Decision when the ball reaches a paddle: "Did the ball touch the paddle?"
        #   Yes -> bounce back at the current speed.
        #   No  -> the ball flies past and the opponent gets the point below.
        # Swept x-collision reduces missed paddle hits on slower computers.
        if self.ball_vx < 0:
            face = LEFT_X + PADDLE_WIDTH + BALL_RADIUS
            if old_x >= face >= self.ball_x:
                t = (old_x - face) / max(old_x - self.ball_x, 1e-9)
                hit_y = old_y + (self.ball_y - old_y) * t
                if self.paddles[LEFT] - BALL_RADIUS <= hit_y <= (
                    self.paddles[LEFT] + PADDLE_HEIGHT + BALL_RADIUS
                ):
                    self.bounce(LEFT, hit_y, face)
        elif self.ball_vx > 0:
            face = RIGHT_X - BALL_RADIUS
            if old_x <= face <= self.ball_x:
                t = (face - old_x) / max(self.ball_x - old_x, 1e-9)
                hit_y = old_y + (self.ball_y - old_y) * t
                if self.paddles[RIGHT] - BALL_RADIUS <= hit_y <= (
                    self.paddles[RIGHT] + PADDLE_HEIGHT + BALL_RADIUS
                ):
                    self.bounce(RIGHT, hit_y, face)

        if self.ball_x < -BALL_RADIUS:
            self.point_for(RIGHT, now)
        elif self.ball_x > GAME_WIDTH + BALL_RADIUS:
            self.point_for(LEFT, now)

    def bounce(self, side, hit_y, face):
        # Offset from paddle midpoint changes return direction.
        center = self.paddles[side] + PADDLE_HEIGHT / 2
        offset = clamp((hit_y - center) / (PADDLE_HEIGHT / 2), -1, 1)
        speed = self.ball_speed
        self.ball_vx = speed * (1 if side == LEFT else -1)
        # Scale the vertical part too, so return angles stay the same when faster.
        self.ball_vy = offset * 290 * speed / BALL_SPEED
        self.ball_x = face + (2 if side == LEFT else -2)


def draw_text(img, text, position, size=0.65, color=WHITE, thickness=2):
    cv2.putText(
        img, text, position, cv2.FONT_HERSHEY_SIMPLEX,
        size, color, thickness, cv2.LINE_AA,
    )


def draw_centered(img, text, y, size, color, thickness):
    (w, _), _ = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, size, thickness)
    draw_text(img, text, ((img.shape[1] - w) // 2, y), size, color, thickness)


def render_menu(fps):
    """Start screen: choose single-player or two-player mode."""
    canvas = np.zeros((GAME_HEIGHT, GAME_WIDTH, 3), dtype=np.uint8)
    canvas[:] = (22, 22, 27)
    draw_centered(canvas, "YOLO GESTURE PONG", 90, 1.1, YELLOW, 2)
    draw_centered(canvas, "Choose a mode", 135, 0.6, WHITE, 1)

    options = (
        ("1", "SINGLE PLAYER", BLUE,
         "One person: left hand = left paddle, right hand = right paddle"),
        ("2", "TWO PLAYERS", GREEN,
         "Left player uses left hand, right player uses right hand"),
    )
    for row, (key, title, color, detail) in enumerate(options):
        y = 200 + row * 110
        cv2.rectangle(canvas, (90, y - 38), (710, y + 42), color, 2)
        draw_text(canvas, f"Press {key}", (115, y), 0.8, color)
        draw_text(canvas, title, (290, y), 0.8, WHITE)
        draw_text(canvas, detail, (115, y + 28), 0.45, (190, 190, 190), 1)

    draw_text(canvas, f"FPS: {fps:.1f}    Q quit  |  M menu (in game)",
              (15, GAME_HEIGHT - 7), 0.45, (175, 175, 175), 1)
    return canvas


def crop_winner(frame, box=None, pad=0.12):
    """Return an in-memory copy of the winner's part of the frame (never saved).

    box is the winner's (x1, y1, x2, y2) detection box, padded a little so
    the whole upper body fits; with no box, the whole frame is used.
    """
    if box is None:
        return frame.copy()
    height, width = frame.shape[:2]
    x1, y1, x2, y2 = (float(v) for v in box[:4])
    pad_x, pad_y = (x2 - x1) * pad, (y2 - y1) * pad
    x1, x2 = int(max(0, x1 - pad_x)), int(min(width, x2 + pad_x))
    y1, y2 = int(max(0, y1 - pad_y)), int(min(height, y2 + pad_y))
    if x2 - x1 < 2 or y2 - y1 < 2:
        return frame.copy()
    return frame[y1:y2, x1:x2].copy()


def fit_image(img, max_width, max_height):
    """Resize to fit inside max_width x max_height, keeping the aspect ratio."""
    height, width = img.shape[:2]
    scale = min(max_width / width, max_height / height)
    size = (max(1, int(width * scale)), max(1, int(height * scale)))
    return cv2.resize(img, size, interpolation=cv2.INTER_AREA)


def render_winner(canvas, message, sub, photo, color):
    """Winner panel: the photo from the winning moment, framed, above the message."""
    shade = canvas.copy()
    cv2.rectangle(shade, (75, 66), (725, 478), (12, 12, 18), -1)
    cv2.addWeighted(shade, 0.88, canvas, 0.12, 0, canvas)
    thumb = fit_image(photo, 420, 300)
    h, w = thumb.shape[:2]
    x0, y0 = (GAME_WIDTH - w) // 2, 80
    canvas[y0:y0 + h, x0:x0 + w] = thumb
    cv2.rectangle(canvas, (x0 - 3, y0 - 3), (x0 + w + 2, y0 + h + 2), color, 3)
    draw_centered(canvas, message, y0 + h + 42, 0.9, YELLOW, 2)
    draw_centered(canvas, sub, y0 + h + 72, 0.49, WHITE, 1)
    return canvas


def render_game(game, hands_ready, paused, fps, now, mode, winner_photo=None):
    canvas = np.zeros((GAME_HEIGHT, GAME_WIDTH, 3), dtype=np.uint8)
    canvas[:] = (22, 22, 27)
    cv2.rectangle(canvas, (0, TOP), (GAME_WIDTH - 1, BOTTOM), GRAY, 1)
    for y in range(TOP + 10, BOTTOM - 10, 28):
        cv2.line(canvas, (GAME_WIDTH // 2, y),
                 (GAME_WIDTH // 2, y + 14), (80, 80, 80), 2)
    names = SIDE_NAMES[mode]
    draw_text(canvas, names[LEFT], (60, 34), 0.65, BLUE)
    draw_text(canvas, str(game.scores[LEFT]), (305, 37), 1.0, BLUE)
    draw_text(canvas, str(game.scores[RIGHT]), (465, 37), 1.0, GREEN)
    draw_text(canvas, names[RIGHT], (600, 34), 0.65, GREEN)
    multiplier = game.ball_speed / BALL_SPEED
    current_round = min(game.rounds_played + 1, MATCH_ROUNDS)
    status = f"ROUND {current_round}/{MATCH_ROUNDS}   SPEED x{multiplier:.2f}"
    if game.winner is not None:
        status = f"MATCH OVER   {game.scores[LEFT]} - {game.scores[RIGHT]}"
    elif game.ball_speed < MAX_BALL_SPEED:
        status += f"   faster in {game.seconds_to_speed_up:.0f}s"
    else:
        status += "   MAX"
    color = YELLOW if game.speed_level > 0 else (175, 175, 175)
    (w, _), _ = cv2.getTextSize(status, cv2.FONT_HERSHEY_SIMPLEX, 0.45, 1)
    draw_text(canvas, status, ((GAME_WIDTH - w) // 2, 50), 0.45, color, 1)

    for x, y, color in zip(
        (LEFT_X, RIGHT_X), game.paddles, (BLUE, GREEN)
    ):
        cv2.rectangle(canvas, (int(x), int(y)),
                      (int(x + PADDLE_WIDTH), int(y + PADDLE_HEIGHT)),
                      color, -1)
    cv2.circle(canvas, (int(game.ball_x), int(game.ball_y)),
               BALL_RADIUS, WHITE, -1, cv2.LINE_AA)
    draw_text(canvas, f"FPS: {fps:.1f}    Q quit  |  R restart  |  P pause  |  M menu",
              (15, GAME_HEIGHT - 7), 0.45, (175, 175, 175), 1)

    if game.winner is not None:
        message = f"{names[game.winner]} WINS!"
        sub = "Press R for a rematch or M for the menu"
        if winner_photo is not None:
            color = BLUE if game.winner == LEFT else GREEN
            return render_winner(canvas, message, sub, winner_photo, color)
    elif not all(hands_ready):
        missing = [name for name, ok in zip(names, hands_ready) if not ok]
        message = "WAITING FOR " + " & ".join(missing)
        if mode == SINGLE_PLAYER:
            sub = "Stand in front of the camera; raise both wrists into view"
        else:
            sub = "Stand side by side: left player raises left hand, right player right hand"
    elif paused:
        message, sub = "PAUSED", "Press P to continue"
    elif game.serve_at is not None:
        message = "GET READY"
        sub = f"Serve in {max(0.0, game.serve_at - now):.1f} s"
    else:
        return canvas

    shade = canvas.copy()
    cv2.rectangle(shade, (75, 178), (725, 313), (12, 12, 18), -1)
    cv2.addWeighted(shade, 0.82, canvas, 0.18, 0, canvas)
    (w, _), _ = cv2.getTextSize(message, cv2.FONT_HERSHEY_SIMPLEX, 0.8, 2)
    draw_text(canvas, message, ((GAME_WIDTH - w) // 2, 228), 0.8, YELLOW)
    (w, _), _ = cv2.getTextSize(sub, cv2.FONT_HERSHEY_SIMPLEX, 0.49, 1)
    draw_text(canvas, sub, ((GAME_WIDTH - w) // 2, 272), 0.49, WHITE, 1)
    return canvas


def render_preview(result, frame, wrists, box_labels, footer, ranges=()):
    """Show YOLO skeleton, player labels, and a bright dot on each controlling wrist.

    box_labels is a list of (detection index or None, text, color).
    ranges is an optional (left, right) pair of learned (min_y, max_y) wrist
    ranges, drawn as bars at the frame edges: the paddle reaches TOP/BOTTOM
    once the wrist dot passes the inner ticks.
    """
    annotated = result.plot(boxes=True, labels=False, conf=False)
    width = annotated.shape[1]
    for (y_low, y_high), color, x in zip(ranges, (BLUE, GREEN), (6, width - 7)):
        margin = (y_high - y_low) * WRIST_RANGE_EDGE_MARGIN
        cv2.line(annotated, (x, int(y_low)), (x, int(y_high)), color, 4)
        for y in (y_low + margin, y_high - margin):
            cv2.line(annotated, (x - 6, int(y)), (x + 6, int(y)), WHITE, 2)
    for index, text, color in box_labels:
        if index is not None and result.boxes is not None:
            box = result.boxes.xyxy[index].cpu().numpy()
            cv2.putText(annotated, text, (int(box[0]), max(22, int(box[1]) - 5)),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, color, 2)
    for wrist, color in zip(wrists, (BLUE, GREEN)):
        if wrist is not None:
            cv2.circle(annotated, wrist, 11, color, -1, cv2.LINE_AA)
            cv2.circle(annotated, wrist, 14, WHITE, 2, cv2.LINE_AA)
    view = np.zeros((GAME_HEIGHT, PREVIEW_WIDTH, 3), dtype=np.uint8)
    view[:] = (34, 34, 39)
    scaled_height = round(frame.shape[0] * PREVIEW_WIDTH / frame.shape[1])
    thumb = cv2.resize(annotated, (PREVIEW_WIDTH, scaled_height))
    y0 = 66
    visible_height = min(scaled_height, GAME_HEIGHT - y0 - 40)
    view[y0:y0 + visible_height] = thumb[:visible_height]
    draw_text(view, "LIVE YOLO POSE", (75, 34), 0.75, WHITE)
    draw_centered(view, footer, GAME_HEIGHT - 19, 0.5, WHITE, 1)
    return view


def main():
    from ultralytics import YOLO

    print("Loading YOLO pose model (first run downloads the weights)...")
    model = YOLO(MODEL_NAME)
    cam = cv2.VideoCapture(CAMERA_INDEX)
    cam.set(cv2.CAP_PROP_FRAME_WIDTH, CAMERA_WIDTH)
    cam.set(cv2.CAP_PROP_FRAME_HEIGHT, CAMERA_HEIGHT)
    if not cam.isOpened():
        raise RuntimeError(
            "Webcam not available. Check permissions or change CAMERA_INDEX to 1."
        )

    game = Pong()
    mode = None  # None = start menu; else SINGLE_PLAYER or TWO_PLAYER.
    paused = False
    last_time = time.monotonic()
    fps = 0.0
    hands = None  # Created from the first frame's real height.
    winner_photo = None  # Shown on the win screen; kept in memory only, never saved.
    cv2.namedWindow(WINDOW_NAME, cv2.WINDOW_NORMAL)
    print("Choose a mode in the game window: 1 = single player, 2 = two players.")
    print("Controls: Q quit | R restart | P pause/resume | M back to menu")

    try:
        while True:
            success, frame = cam.read()
            if not success:
                print("Camera stopped providing frames.")
                break
            frame = cv2.flip(frame, 1)  # Mirror the view for natural controls.
            if hands is None:
                hands = [HandTracker(frame.shape[0]), HandTracker(frame.shape[0])]

            now = time.monotonic()
            dt = now - last_time
            last_time = now
            fps = 0.85 * fps + 0.15 * (1 / max(dt, 1e-4))

            result = model(frame, imgsz=INFERENCE_SIZE,
                           conf=0.35, max_det=5, verbose=False)[0]
            if mode is None:
                game_image = render_menu(fps)
                preview = render_preview(result, frame, [None, None], [],
                                         "PRESS 1 OR 2 TO START")
            else:
                if mode == SINGLE_PLAYER:
                    points, body_center_x, player_index = detect_player_wrists(result)
                    previous = [h.last_point if h.is_live(now) else None for h in hands]
                    new_wrists = split_wrists(points, body_center_x, previous)
                    box_labels = [(player_index, "PLAYER", WHITE)]
                    footer = "LEFT HAND = BLUE   RIGHT HAND = GREEN"
                else:
                    new_wrists, indexes = detect_two_player_wrists(result, frame.shape[1])
                    box_labels = [(indexes[LEFT], "P1", BLUE), (indexes[RIGHT], "P2", GREEN)]
                    footer = "LEFT = P1 (left hand)   RIGHT = P2 (right hand)"

                for side, (hand, wrist) in enumerate(zip(hands, new_wrists)):
                    if wrist is not None:
                        game.paddles[side] = hand.observe(wrist, now)
                    # A brief dropout keeps the paddle where it was (hold).
                ready = [hand.is_live(now) for hand in hands]
                wrists = [hand.last_point if ok else None for hand, ok in zip(hands, ready)]

                # Stop both the ball AND the serve timer when tracking is lost.
                active = all(ready) and not paused
                if active:
                    had_winner = game.winner is not None
                    game.update(dt, now)
                    if game.winner is not None and not had_winner:
                        # Grab the winner from this last frame (memory only).
                        if mode == SINGLE_PLAYER:
                            winner_index = player_index
                        else:
                            winner_index = indexes[game.winner]
                        box = (result.boxes.xyxy[winner_index].cpu().numpy()
                               if winner_index is not None else None)
                        winner_photo = crop_winner(frame, box)
                elif game.serve_at is not None:
                    game.serve_at += dt

                game_image = render_game(game, ready, paused, fps, now, mode,
                                         winner_photo)
                preview = render_preview(result, frame, wrists, box_labels, footer,
                                         [hand.reach.range for hand in hands])
            cv2.imshow(WINDOW_NAME, np.hstack([game_image, preview]))

            key = cv2.waitKey(1) & 0xFF
            if key == ord("q") or key == 27:
                break
            if mode is None:
                if key in (ord("1"), ord("2")):
                    mode = SINGLE_PLAYER if key == ord("1") else TWO_PLAYER
                    key = ord("r")  # Start the chosen mode fresh.
                else:
                    continue
            if key == ord("m"):
                mode = None
                paused = False
                winner_photo = None
            if key == ord("r"):
                game.restart()
                paused = False
                winner_photo = None
                for hand in hands:
                    hand.reset()
            if key == ord("p"):
                paused = not paused
    finally:
        cam.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
