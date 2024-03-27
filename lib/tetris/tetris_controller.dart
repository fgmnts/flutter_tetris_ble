import 'dart:async';
import 'dart:convert';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'providers/BluetoothProvider.dart';
import 'configs.dart';
import 'feature/tetris_methods/methods.dart';
import 'model/enum/game_info.dart';
import 'model/models.dart';

class _InheritedTetris extends InheritedWidget {
  const _InheritedTetris({
    required this.data,
    required super.child,
  });

  final TetrisControllerState data;

  @override
  bool updateShouldNotify(covariant InheritedWidget oldWidget) => true;
}

class TetrisController extends StatefulWidget {
  const TetrisController({
    super.key,
    required this.child,
    required this.bleProvider,
  });

  final Widget child;
  final BluetoothProvider bleProvider;

  static TetrisControllerState of(BuildContext context) {
    return context.dependOnInheritedWidgetOfExactType<_InheritedTetris>()!.data;
  }

  @override
  State<TetrisController> createState() => TetrisControllerState();
}

class TetrisControllerState extends State<TetrisController> {
  // late BluetoothProvider bleProvider;
  Panels fieldState = [];
  MinoStateModel currentMinoStateModel = MinoStateModel.init();
  List<MinoConfig> nextMinos = [];
  MinoConfig? keepMino;
  int score = 0;
  bool isTspin = false;
  bool isKept = false;
  GameInfo gameInfo = GameInfo.waiting;
  Timer? timer;

  StreamSubscription<dynamic>? bleScanSubscription;
  StreamSubscription<BluetoothConnectionState>? bleStreamSubscription;

  bool get isPlaying => gameInfo == GameInfo.playing;

  @override
  void initState() {
    init();
    super.initState();
  }

  @override
  void dispose() {
    bleStreamSubscription?.cancel();
    bleScanSubscription?.cancel();
    super.dispose();
  }

  void init() {
    fieldState = getInitialField();
    nextMinos.clear();
    nextMinos = setNextMinos(nextMinos: nextMinos);
    keepMino = null;
    score = 0;
    isTspin = false;
    isKept = false;
    gameInfo = GameInfo.waiting;
    scanForBluetoothDevices();
  }

  // Bluetooth verbinding maken
  Future<void> scanForBluetoothDevices() async {
    print("SCAN FOR BLUETOOTH DEVICES");
    bleScanSubscription = FlutterBluePlus.onScanResults.listen(
      (results) {
        if (results.isNotEmpty) {
          final ScanResult r = results.last; // the most recently found device
          print(
            '${r.device.remoteId}: "${r.advertisementData.advName}" found!',
          );

          connectToBluetoothDevice(r.device);
        }
      },
      onError: (e) => print(e),
    );

    FlutterBluePlus.cancelWhenScanComplete(bleScanSubscription!);

    await FlutterBluePlus.adapterState
        .where((val) => val == BluetoothAdapterState.on)
        .first;

    await FlutterBluePlus.startScan(
      withRemoteIds: ['40:22:D8:08:4A:5A'],
      timeout: const Duration(seconds: 15),
    );
  }

  Future<void> connectToBluetoothDevice(BluetoothDevice device) async {
// listen for disconnection
    bleStreamSubscription =
        device.connectionState.listen((BluetoothConnectionState state) async {
      if (state == BluetoothConnectionState.disconnected) {
        print("Disconnected");
        // 1. typically, start a periodic timer that tries to
        //    reconnect, or just call connect() again right now
        // 2. you must always re-discover services after disconnection!
        // print("${device.disconnectReasonCode} ${device.disconnectReasonDescription}");
      }
    });

// cleanup: cancel subscription when disconnected
// Note: `delayed:true` lets us receive the `disconnected` event in our handler
// Note: `next:true` means cancel on *next* disconnection. Without this, it
//   would cancel immediately because we're already disconnected right now.
    device.cancelWhenDisconnected(
      bleStreamSubscription!,
      delayed: true,
      next: true,
    );
    await device.connect();

    unawaited(findAndSubscribeUARTService(device));
  }

  Future<void> findAndSubscribeUARTService(BluetoothDevice device) async {
// Note: You must call discoverServices after every re-connection!
    List<BluetoothService> services = await device.discoverServices();
    services.forEach((service) async {
      if (service.serviceUuid.toString().toUpperCase() ==
          '6E400001-B5A3-F393-E0A9-E50E24DCCA9E') {
        print("UART service found");
        const txCharacteristicUuid = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E';

        var characteristics = service.characteristics;
        var txCharacteristic = characteristics.firstWhere(
          (element) =>
              element.characteristicUuid.toString().toUpperCase() ==
              txCharacteristicUuid,
          orElse: () => throw Exception('TX characteristic not found'),
        ); // Error handling)

        final subscription = txCharacteristic.onValueReceived.listen((value) {
          String msg = utf8.decode(value);
          print("a new value is received via BLE: $value");
          print('as string? ${msg}');

          widget.bleProvider.pushMessage(msg);
          // onValueReceived is updated:
          //   - anytime read() is called
          //   - anytime a notification arrives (if subscribed)
        });

// cleanup: cancel subscription when disconnected
        device.cancelWhenDisconnected(subscription);

// subscribe
// Note: If a characteristic supports both **notifications** and **indications**,
// it will default to **notifications**. This matches how CoreBluetooth works on iOS.
        await txCharacteristic.setNotifyValue(true);
      }
    });
  }

  void left() {
    if (!isPlaying) {
      return;
    }
    final setResult = moveLeft(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );

    if (setResult.$1) {
      setState(() {
        fieldState = setResult.$2!;
      });
      currentMinoStateModel = currentMinoStateModel.moveLeft();
    }
  }

  void right() {
    if (!isPlaying) {
      return;
    }
    final setResult = moveRight(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );

    if (setResult.$1) {
      setState(() {
        fieldState = setResult.$2!;
      });
      currentMinoStateModel = currentMinoStateModel.moveRight();
    }
  }

  void down() {
    if (!isPlaying) {
      return;
    }

    final setResult = moveDown(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );

    if (setResult.$1) {
      setState(() {
        fieldState = setResult.$2!;
      });
      currentMinoStateModel = currentMinoStateModel.moveDown();
      isTspin = false;
    } else {
      deletePanels();
      isTspin = false;
      isKept = false;
      add();
    }
  }

  void hardDrop() {
    if (!isPlaying) {
      return;
    }
    final loopResult = hardDropLoop(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );
    setState(() {
      currentMinoStateModel = loopResult.$1;
      fieldState = loopResult.$2;
    });
    down();
  }

  void r90() {
    if (!isPlaying) {
      return;
    }
    final currentMino = currentMinoStateModel.config;

    if (currentMino == MinoConfig.t) {
      final tSpinResult = tSpinSetR90(
        currentMinoStateModel: currentMinoStateModel,
        fieldState: fieldState,
      );

      if (tSpinResult.$1) {
        setState(() {
          fieldState = tSpinResult.$2!;
        });
        currentMinoStateModel = tSpinResult.$3!;
        isTspin = true;
      }
      return;
    }

    final setResult = rotateR90(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );

    if (setResult.$1) {
      setState(() {
        fieldState = setResult.$2!;
      });

      currentMinoStateModel = currentMinoStateModel.rotateR90();
    }
  }

  void l90() {
    if (!isPlaying) {
      return;
    }
    final currentMino = currentMinoStateModel.config;

    if (currentMino == MinoConfig.t) {
      final tSpinResult = tSpinSetL90(
        currentMinoStateModel: currentMinoStateModel,
        fieldState: fieldState,
      );

      if (tSpinResult.$1) {
        setState(() {
          fieldState = tSpinResult.$2!;
        });
        currentMinoStateModel = tSpinResult.$3!;
        isTspin = true;
      }
      return;
    }

    final setResult = rotateL90(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );

    if (setResult.$1) {
      setState(() {
        fieldState = setResult.$2!;
      });

      currentMinoStateModel = currentMinoStateModel.rotateL90();
    }
  }

  void add() {
    final nextMinoStateModel = MinoStateModel(
      config: nextMinos.first,
      position: PositionModel.init(),
      rotation: Rotation.r0,
    );

    final setResult = setMino(
      minoStateModel: nextMinoStateModel,
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
    );

    if (setResult.$1) {
      currentMinoStateModel = nextMinoStateModel;

      nextMinos.removeAt(0);

      nextMinos = setNextMinos(nextMinos: nextMinos);
    } else {
      stop();
    }
  }

  void deletePanels() {
    final deletePanelsResult = deletePanelsMethod(fieldState: fieldState);

    score += (deletePanelsResult.$1) *
        (deletePanelsResult.$1) *
        scoreUnit *
        (isTspin ? tspinBonus : 1);

    setState(() {
      fieldState = deletePanelsResult.$2;
    });
  }

  void start() {
    init();
    timer = Timer.periodic(
      const Duration(
        milliseconds: initialDurationMillisecconds,
      ),
      (timer) => down(),
    );
    gameInfo = GameInfo.playing;
  }

  void stop() {
    if (timer != null && isPlaying) {
      timer!.cancel();
    }
    timer!.cancel();
    gameInfo = GameInfo.gameOver;
  }

  void keep() {
    if (!isPlaying) {
      return;
    }
    if (isKept) {
      return;
    }

    final keepResult = keepMethod(
      currentMinoStateModel: currentMinoStateModel,
      fieldState: fieldState,
      keepMino: keepMino,
      nextMinos: nextMinos,
    );

    if (keepResult.$1) {
      setState(() {
        fieldState = keepResult.$2!;
      });
      if (keepMino != null) {
        final tempMino = keepMino;
        keepMino = currentMinoStateModel.config;
        currentMinoStateModel = currentMinoStateModel.copyWith(
          rotation: Rotation.r0,
          config: tempMino,
        );
      } else {
        keepMino = currentMinoStateModel.config;
        currentMinoStateModel = currentMinoStateModel.copyWith(
          rotation: Rotation.r0,
          config: nextMinos.first,
        );
        nextMinos.removeAt(0);
        nextMinos = setNextMinos(nextMinos: nextMinos);
      }
      isKept = true;
    }
  }

  @override
  Widget build(BuildContext context) {
    return _InheritedTetris(
      data: this,
      child: widget.child,
    );
  }
}
