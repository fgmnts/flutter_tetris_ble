import 'package:flutter/material.dart';

enum BluetoothStatus { disconnected, connecting, connected }

class BluetoothProvider with ChangeNotifier {
  BluetoothStatus _status = BluetoothStatus.disconnected;
  final List<String> _messages = [];

  BluetoothStatus get status => _status;
  List<String> get messages => _messages;
  String? get lastMessage => _messages.last;
  void pushMessage(String msg) {
    _messages.add(msg);
    notifyListeners();
  }

  void setStatus(BluetoothStatus newStatus) {
    _status = newStatus;
    notifyListeners();
  }
}
