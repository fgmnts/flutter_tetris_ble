import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import 'configs.dart';
import 'model/enum/tetris_colors.dart';
import 'providers/BluetoothProvider.dart';
import 'tetris_controller.dart';
import 'widgets/buttons.dart';
import 'widgets/game_info_widget.dart';
import 'widgets/widgets.dart';

class FlutterTetris extends StatelessWidget {
  const FlutterTetris({super.key});

  @override
  Widget build(BuildContext context) {
    return ChangeNotifierProvider(
      create: (_) => BluetoothProvider(),
      builder: (context, child) {
        return TetrisController(
          bleProvider: Provider.of<BluetoothProvider>(context, listen: false),
          child: MaterialApp(
            theme: ThemeData(
              colorSchemeSeed: TetrisColors.deepBlue.color,
              elevatedButtonTheme: const ElevatedButtonThemeData(
                style: ButtonStyle(
                  shape: MaterialStatePropertyAll(LinearBorder.none),
                ),
              ),
            ),
            home: Builder(
              builder: (context) {
                return KeyboardInputWidget(
                  child: Scaffold(
                    backgroundColor: TetrisColors.deepBlue.color,
                    body: Center(
                      child: Column(
                        mainAxisAlignment: MainAxisAlignment.center,
                        children: [
                          const Row(
                            crossAxisAlignment: CrossAxisAlignment.start,
                            mainAxisAlignment: MainAxisAlignment.center,
                            children: [
                              Column(
                                children: [
                                  KeepMino(),
                                  SizedBox(
                                    height: spaceSize,
                                  ),
                                  Score(),
                                ],
                              ),
                              Stack(
                                alignment: Alignment.center,
                                children: [
                                  Field(),
                                  GameInfoWidget(),
                                ],
                              ),
                              NextMinos(),
                            ],
                          ),
                          const SizedBox(
                            height: spaceSize,
                          ),
                          const Buttons(),
                          Consumer<BluetoothProvider>(
                            builder: (context, provider, child) {
                              // Update text based on status:
                              // String statusText;
                              // switch (provider.status) {
                              //   case BluetoothStatus.disconnected:
                              //     statusText = 'BLE not connected';
                              //   case BluetoothStatus.connecting:
                              //     statusText = 'BLE connecting...';
                              //   case BluetoothStatus.connected:
                              //     statusText = 'BLE connected';
                              // }
                              return Column(
                                children: [
                                  // Text(
                                  //   statusText,
                                  //   style: TextStyle(color: Colors.white),
                                  // ),
                                  if (provider.messages.isNotEmpty) ...[
                                    Text(
                                      'Last message: ${provider.messages.last}',
                                      style:
                                          const TextStyle(color: Colors.white),
                                    ),
                                    const Text(
                                      'hello',
                                      style: TextStyle(color: Colors.white),
                                    )
                                  ]
                                ],
                              );
                            },
                          ),
                          const Text(
                            'hello UI',
                            style: TextStyle(color: Colors.white),
                          )
                        ],
                      ),
                    ),
                  ),
                );
              },
            ),
          ),
        );
      },
    );
  }
}
