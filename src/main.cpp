/*
 * Test đơn giản MCP2515 và CAN Bus
 */

#include <SPI.h>

#define CAN_CS_PIN 53 // Chân CS cho CAN module (MCP2515), kéo chân này xuống mức thấp để giao tiếp với module
#define CAN_INT_PIN 2 // Chân interrupt cho CAN (MCP2515), chân này được module sử dụng để báo cho vi điều khiển khi có 1 sự kiện xảy ra

void testMCP2515() {
  Serial.println("Testing MCP2515");
  
  // Reset MCP2515
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(0xC0);  // Reset command
  digitalWrite(CAN_CS_PIN, HIGH);
  delay(10);
  
  // Đọc CANSTAT register
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(0x03);  // Read command
  SPI.transfer(0x0E);  //  Read CANSTAT address
  uint8_t canstat = SPI.transfer(0x00); // Đọc dữ liệu từ CANSTAT, gửi đi 0x00 để tạo xung SCK để đọc dữ liệu
  digitalWrite(CAN_CS_PIN, HIGH); // unit8 là kiểu dữ liệu 8 bit
  
  Serial.print("Giá trị của CANSTAT: 0x");
  Serial.println(canstat, HEX);
  
  if (canstat == 0x80) {
    Serial.println("✅ MCP2515 OK!"); // Check với điều kiện MCP2515 ở trạng thái config mode
  } else {
    Serial.println("❌ MCP2515 có vấn đề!");
    Serial.println("Kiểm tra:");
    Serial.println("- Kết nối SPI");
    Serial.println("- Nguồn điện 5V");
    Serial.println("- Chân CS = 53");
  }
}

void sendCANCommand() {
  Serial.println("Gửi test CAN message...");

  // Chuẩn bị data
  uint8_t data[8] = {0x9A, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}; // 8 byte dữ liệu test read motor status

  // Ghi vào TX buffer
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(0X40); // Load tx buffer 0
  SPI.transfer(0x28); //ID high of 0x141, command 0X140 cộng với ID là 1, TXB0SIDH
  SPI.transfer(0x20); //ID low of 0x141, command 0x140 cộng với ID là 1, TXB0SIDL
  SPI.transfer(0x00); // Extended ID high TXB0EID8, set extended bit = 0 để bỏ qua 
  SPI.transfer(0x00); // Extended ID low TXB0EID0, set extended bit = 0 để bỏ qua 
  SPI.transfer(0x08);  // Data length = 8 bits

  // Ghi data
  for(int i = 0; i < 8; i++) {
    SPI.transfer(data[i]);
  }
  digitalWrite(CAN_CS_PIN, HIGH);

  delay(1);

  // Trigger transmission
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(0x81);  // Request to send data in TXB0
  digitalWrite(CAN_CS_PIN, HIGH);
  Serial.println("Đã gửi lệnh đọc trạng thái motor 0x9A");
  Serial.println("Chờ motor phản hồi");
  delay(5);

  // Đọc trạng thái của CANINTF register
  digitalWrite(CAN_CS_PIN, LOW);
  SPI.transfer(0x03);  // Read command
  SPI.transfer(0x2C);  // Read CANINTF register
  uint8_t intFlags = SPI.transfer(0x00); // Đọc dữ liệu từ CANINTF, gửi đi 0x00 để tạo xung SCK để đọc dữ liệu, kiểu như là muốn đọc được dữ liệu thì phải gửi dữ liệu dummy để được nhận dữ liệu vể
  digitalWrite(CAN_CS_PIN, HIGH); // Bỏ chọn MCP2515 hay ngắt chân CS để ngắt kết nối với module MCP2515
  
 // Kiểm tra xem có message trong RX buffer không
 // Bit 0 của CANINTF register là RX0IF, nếu bằng 1 thì có 1 interrupt pending, có nghĩa là có 1 message trong RX buffer
  if (intFlags & 0x01) {
    Serial.println("Có message trong RX buffer");
    uint8_t receivedData[8];

    // Khởi tạo một giao tiếp mới để đọc dữ liệu
    digitalWrite(CAN_CS_PIN, LOW);
    SPI.transfer(0x03); // Read command
    SPI.transfer(0x66); // Địa chỉ 0x66 là địa chỉ của RXB0D0, tức là byte đầu tiên của RX buffer, có thể dùng 0x92 là 10010010 là khỏi dùng lệnh read

    // Đọc dữ liệu
    for(int i = 0; i < 8; i++) {
      receivedData[i] = SPI.transfer(0x00);
    }
    digitalWrite(CAN_CS_PIN, HIGH);

    // Giải mã và in dữ liệu nhận được
    if (receivedData[0] != 0x9A) {
      Serial.println("❌ Message không phải là phản hồi của lệnh đọc trạng thái motor");
      return;
    } else {
      Serial.println("✅ Message nhận được là phản hồi của lệnh đọc trạng thái motor");

      // Giá trị nhiệt độ
      Serial.println("Giải mã dữ liệu");
      uint8_t temperature = uint8_t(receivedData[1]); // uint8_t là kiểu dữ liệu 8 bit không dấu từ 0 đến 255
      Serial.print("1. Nhiệt độ : ");
      Serial.print(temperature);
      Serial.println("°C");

      // Trạng thái phanh
      Serial.print("2. Trạng thái phanh: ");
      if (receivedData[3] == 1) {
        Serial.println("Đã nhả (Released)");
      } else {
        Serial.println("Đang giữ (Braking)");
      }

      // Điện áp
      uint8_t voltageLowByte = receivedData[4];   // DATA[4] - Voltage low byte
      uint8_t voltageHighByte = receivedData[5];  // DATA[5] - Voltage high byte
      uint16_t voltage = (voltageHighByte << 8) | voltageLowByte; // Kết hợp high byte và low byte

      Serial.print("3. Điện áp: ");
      Serial.print(voltage);
      Serial.println("V");

      // Error status 
      uint8_t errorStatusLow = receivedData[6];   // DATA[6] - Error status low byte 1
      uint8_t errorStatusHigh = receivedData[7];  // DATA[7] - Error status byte 2
      uint16_t errorStatus = (errorStatusHigh << 8) | errorStatusLow;

      // Mỗi số không đại diện cho 4 bit, được viết dưới dạng thập lục phân
      Serial.print("4. Trạng thái lỗi hiện tại: ");
      if (errorStatus == 0x0000) {
        Serial.println("Không có lỗi");
      }
      else if (errorStatus == 0x0002) {
        Serial.println("Motor Stall");
      } 
      else if (errorStatus == 0x0004) {
        Serial.println("Low Voltage");
      }
      else if (errorStatus == 0x0008) {
        Serial.println("Over Voltage");
      }
      else if (errorStatus == 0x0010) {
        Serial.println("Over Current");
      }
      else if (errorStatus == 0x0020) {
        Serial.println("Over Speed");
      }
      else if (errorStatus == 0x0040) {
        Serial.println("Power Overrun");
      }
      else if (errorStatus == 0x0080) {
        Serial.println("Calibration Parameter Wiring Error");
      }
      else if (errorStatus == 0x1000) {
        Serial.println("Motor Temperature Over Temperature");
      }
      else if (errorStatus == 0x2000) {
        Serial.println("Encoder Calibration Error");
      }
      else {
        Serial.print("Lỗi không xác định: 0x");
        Serial.println(errorStatus, HEX);
      }
    }
  } else {
    // Không có message trong RX buffer 0
    Serial.println("Không có message trong RX buffer");
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("Test MCP2515");
  
  // Khởi tạo SPI
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV16); // 1MHz, tốc độ tối đa của MCP2515 cho giao thức SPI là 10 MHz
  SPI.setDataMode(SPI_MODE0); // Chế độ đồng bộ clock/data, khi nghỉ ở mức 0 và khi đọc dữ liệu là cạnh lên từ 0 lên 5V
  SPI.setBitOrder(MSBFIRST); // Thiết lập thứ tự truyền bit trong SPI
  
  pinMode(CAN_CS_PIN, OUTPUT);
  pinMode(CAN_INT_PIN, INPUT);
  digitalWrite(CAN_CS_PIN, HIGH); // Bỏ chọn MCP2515
  
  Serial.println("SPI initialized");
  
  // Test MCP2515
  testMCP2515();
  
  Serial.println("Gõ 's' để test gửi CAN message");
}

void loop() {
  if (Serial.available()) {
    char cmd = Serial.read(); // Đọc 1 ký tự từ Serial, char chứa 8 bit dữ liệu
    if (cmd == 's' || cmd == 'S') {
      sendCANCommand();
    }
  }
}