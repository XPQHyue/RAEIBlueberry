import sensor, image, time, tf, math, uos, gc, lcd

# ── 传感器初始化 ──
sensor.reset()
sensor.set_pixformat(sensor.RGB565)
sensor.set_framesize(sensor.QVGA)
sensor.set_windowing((240, 240))
sensor.set_vflip(True)
sensor.set_hmirror(True)
sensor.skip_frames(time=2000)
lcd.init()

# ── 初始化完成 ──
print("Init OK")

# ── LCD 启动画面 ──
try:
    splash1 = image.Image("splash1.ppm", copy_to_fb=True)
    lcd.display(splash1)
    time.sleep(2)
    splash2 = image.Image("splash2.ppm", copy_to_fb=True)
    lcd.display(splash2)
    time.sleep(2)
except Exception as e:
    print("Splash load failed:", e)

sensor.skip_frames(time=500)

# ── 模型 & 标签 ──
MODEL_PATH = "trained.tflite"
LABELS_PATH = "labels.txt"

print("Loading model...")
try:
    net = tf.load(MODEL_PATH, load_to_fb=uos.stat(MODEL_PATH)[6] > (gc.mem_free() - (64*1024)))
    print("OK  input:%dx%dx%d  output:%dx%dx%d" % (
        net.input_width(), net.input_height(), net.input_channels(),
        net.output_width(), net.output_height(), net.output_channels()))
except Exception as e:
    raise Exception('Failed to load model: ' + str(e))

try:
    labels = [line.rstrip('\n') for line in open(LABELS_PATH)]
    print("Labels:", labels)
except Exception as e:
    raise Exception('Failed to load labels.txt: ' + str(e))

colors = [
    (255,0,0), (0,255,0), (255,255,0),
    (0,0,255), (255,0,255), (0,255,255), (255,255,255),
]

# ── 持久化均值（始终显示） ──
last_avg_cx = 0
last_avg_cy = 0
avg_valid = False

clock = time.clock()
while True:
    clock.tick()
    img = sensor.snapshot()
    detections = net.detect(img)

    # ── 收集所有检测目标的中心坐标 ──
    cx_list = []
    cy_list = []

    for i, dl in enumerate(detections):
        if i == 0:
            continue
        for d in dl:
            x, y, w, h = d.x(), d.y(), d.w(), d.h()
            score = d.output()
            if score < 0.9:
                continue
            cx = x + w // 2
            cy = y + h // 2
            cx_list.append(cx)
            cy_list.append(cy)

            # ── 标签名（仅串口使用） ──
            label_str = labels[i] if i < len(labels) else "obj%d" % i

            # ── 标注圆圈和十字 ──
            img.draw_circle((cx, cy, 12), color=colors[i % len(colors)])
            img.draw_cross(cx, cy, color=colors[i % len(colors)])

            # ── 串口终端打印坐标 ──
            print("********** %s **********" % label_str)
            print("x %d\ty %d\tscore %.2f" % (cx, cy, score))

    # ── 更新均值 ──
    if cx_list:
        last_avg_cx = sum(cx_list) // len(cx_list)
        last_avg_cy = sum(cy_list) // len(cy_list)
        avg_valid = True

    # ── 均值始终显示在屏幕左上角 ──
    if avg_valid:
        avg_str = "AVG x=%d y=%d" % (last_avg_cx, last_avg_cy)
        img.draw_rectangle(0, 0, len(avg_str) * 8 + 6, 20, color=(0,0,0), fill=True)
        img.draw_string(5, 5, avg_str, color=(255,255,255), scale=1)

    lcd.display(img)
    print("%.1f fps" % clock.fps())
