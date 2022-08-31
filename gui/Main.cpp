#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
# include <Siv3D.hpp> // OpenSiv3D v0.6.5

class Pulldown
{
public:

	Pulldown() = default;

	Pulldown(const Array<String>& items, const Font& font, const Point& pos = { 0,0 })
		: m_font{ font }
		, m_items{ items }
		, m_rect{ pos, 0, (m_font.height() + m_padding.y * 2) }
	{
		for (const auto& item : m_items)
		{
			m_rect.w = Max(m_rect.w, static_cast<int32>(m_font(item).region().w));
		}

		m_rect.w += (m_padding.x * 2 + m_downButtonSize);
	}

	bool isEmpty() const
	{
		return m_items.empty();
	}

	void update()
	{
		if (isEmpty())
		{
			return;
		}

		if (m_rect.leftClicked())
		{
			m_isOpen = (not m_isOpen);
		}

		Point pos = m_rect.pos.movedBy(0, m_rect.h);

		if (m_isOpen)
		{
			for (auto i : step(m_items.size()))
			{
				if (const Rect rect{ pos, m_rect.w, m_rect.h };
					rect.leftClicked())
				{
					m_index = i;
					m_isOpen = false;
					break;
				}

				pos.y += m_rect.h;
			}
		}
	}

	void draw() const
	{
		m_rect.draw();

		if (isEmpty())
		{
			return;
		}

		m_rect.drawFrame(1, 0, m_isOpen ? Palette::Orange : Palette::Gray);

		Point pos = m_rect.pos;

		m_font(m_items[m_index]).draw(pos + m_padding, Palette::Black);

		Triangle{ (m_rect.x + m_rect.w - m_downButtonSize / 2.0 - m_padding.x), (m_rect.y + m_rect.h / 2.0),
			(m_downButtonSize * 0.5), 180_deg }.draw(Palette::Black);

		pos.y += m_rect.h;

		if (m_isOpen)
		{
			const Rect backRect{ pos, m_rect.w, (m_rect.h * m_items.size()) };

			backRect.drawShadow({ 1, 1 }, 4, 1).draw();

			for (const auto& item : m_items)
			{
				if (const Rect rect{ pos, m_rect.size };
					rect.mouseOver())
				{
					rect.draw(Palette::Skyblue);
				}

				m_font(item).draw((pos + m_padding), Palette::Black);

				pos.y += m_rect.h;
			}

			backRect.drawFrame(1, 0, Palette::Gray);
		}
	}

	void setPos(const Point& pos)
	{
		m_rect.setPos(pos);
	}

	const Rect& getRect() const
	{
		return m_rect;
	}

	size_t getIndex() const
	{
		return m_index;
	}

	void setIndex(size_t index) {
		m_index = index;
	}

	String getItem() const
	{
		if (isEmpty())
		{
			return{};
		}

		return m_items[m_index];
	}

private:

	Font m_font;

	Array<String> m_items;

	size_t m_index = 0;

	Size m_padding{ 6, 2 };

	Rect m_rect;

	int32 m_downButtonSize = 16;

	bool m_isOpen = false;
};

struct signal {
	BYTE r, g, b, a;
};

class serial {
	HANDLE arduino = 0;
	
public:
	/// @brief arduinoとの通信の初期化
	/// @param com 使用するCOMポート
	/// @return 初期化に成功したか
	bool init(String com) {
		bool Ret;
		ClearPrint();
		if (arduino != 0) {
			CloseHandle(arduino);
		}
		//1.ポートをオープン
		auto wcom = com.toWstr();
		LPCWSTR _com = wcom.c_str();
		arduino = CreateFile(_com, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (arduino == INVALID_HANDLE_VALUE) {
			Print << U"PORT COULD NOT OPEN";
			return false;
		}

		//2.送受信バッファ初期化
		Ret = SetupComm(arduino, 1024, 1024);
		if (!Ret) {
			Print << U"SET UP FAILED";
			CloseHandle(arduino);
			return false;
		}
		Ret = PurgeComm(arduino, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR);
		if (!Ret) {
			Print << U"CLEAR FAILED";
			CloseHandle(arduino);
			return false;
		}

		//3.基本通信条件の設定
		DCB dcb;
		GetCommState(arduino, &dcb);
		dcb.DCBlength = sizeof(DCB);
		dcb.BaudRate = 9600;
		dcb.fBinary = TRUE;
		dcb.ByteSize = 8;
		dcb.fParity = NOPARITY;
		dcb.StopBits = ONESTOPBIT;

		Ret = SetCommState(arduino, &dcb);
		if (!Ret) {
			Print << U"SetCommState FAILED";
			CloseHandle(arduino);
			system("PAUSE");
			return false;
		}
		return true;
	}

	/// @brief arduinoにデータを送る
	/// @param data 送るデータ(BYTE)
	/// @return 遅れたか
	bool send(BYTE data) {
		DWORD dwSendSize;
		if (!WriteFile(arduino, &data, sizeof(data), &dwSendSize, NULL)) {
			Print << U"SEND FAILED";
			CloseHandle(arduino);
			return false;

		}
		return true;
	}

	/// @brief arduinoとの通信を終了する
	/// @return 終了できたか
	bool close() {
		return CloseHandle(arduino);
	}
};

void Main() {

	Scene::SetBackground(ColorF{ 0.8, 0.9, 1.0 });

	// 表示する色
	HSV colorHSV{1.0,0.0,1.0};
	ColorF colorRGB{ 1.0 };

	// 点灯のインターバル
	double interval = 0.5;

	// 色を変えたかのフラグ
	bool slider_flag = false, picker_flag = false;

	// arduinoとのシリアル通信
	serial arduino;

	// COM選択のプルダウン
	size_t com_port = 0;
	const Font font{ 12 };
	const Array<String> items = { U"COM0", U"COM1", U"COM2", U"COM3", U"COM4", U"COM5", U"COM6", U"COM7", U"COM8", U"COM9", U"COM10", U"COM11", U"COM12" };
	bool serial_flag = false;
	for (auto&& i : Range(0, 12)) { // 通信できるポートを探索
		if (serial_flag = arduino.init(items[i])) {
			com_port = i;
			break;
		}
	}

	Pulldown pulldown{ items, font, Point{ 8, 8 } };
	pulldown.setIndex(com_port);
	
	if (serial_flag) {
		arduino.send(0);
		arduino.send(64);
		arduino.send(64);
		arduino.send(64);
	}

	while (System::Update()) {
		pulldown.update();

		// COMを変えたら通信の初期化
		if (com_port != pulldown.getIndex()) {
			com_port = pulldown.getIndex();
			serial_flag = arduino.init(pulldown.getItem());
		}

		if (SimpleGUI::Slider(U"赤{:.0f}"_fmt(colorRGB.r * 255), colorRGB.r, Vec2{ 50, 160 }, 100, 600)) slider_flag = true;
		if (SimpleGUI::Slider(U"緑{:.0f}"_fmt(colorRGB.g * 255), colorRGB.g, Vec2{ 50, 200 }, 100, 600)) slider_flag = true;
		if (SimpleGUI::Slider(U"青{:.0f}"_fmt(colorRGB.b * 255), colorRGB.b, Vec2{ 50, 240 }, 100, 600)) slider_flag = true;
		if (SimpleGUI::Slider(U"明るさ{:.0f}"_fmt(colorRGB.a * 255), colorRGB.a, Vec2{ 50, 280 }, 100, 600)) slider_flag = true;
		SimpleGUI::Slider(U"時間{:.2f}秒"_fmt(interval), interval, Vec2{ 50, 320 }, 100, 600);
		if (SimpleGUI::ColorPicker(colorHSV, Vec2{ 400, 400 }))picker_flag = true;

		// 各値を整数に四捨五入
		colorRGB.r = std::round(colorRGB.r * 255.0) / 255.0;
		colorRGB.g = std::round(colorRGB.g * 255.0) / 255.0;
		colorRGB.b = std::round(colorRGB.b * 255.0) / 255.0;
		interval = std::round(interval * 100.0) / 100.0;

		// 色が変わったら連動させて色を変える
		if (picker_flag) {
			colorRGB = colorHSV;
		}
		else if (slider_flag) {
			colorHSV = colorRGB;
		}

		// 色のプレビュー表示
		Circle{ 400,80,64 }.draw(ColorF{ 0 });
		Circle{ 400,80,60 }.draw(colorRGB);

		// 色を変えたらarduinoに送る
		if (serial_flag && (slider_flag || picker_flag)) {
			BYTE r, g, b;
			r = static_cast<BYTE>(colorRGB.r * 255 * colorRGB.a);
			g = static_cast<BYTE>(colorRGB.g * 255 * colorRGB.a);
			b = static_cast<BYTE>(colorRGB.b * 255 * colorRGB.a);

			arduino.send(0); // プレビューの時はフラグは0
			arduino.send(r);
			arduino.send(g);
			arduino.send(b);
		}
		slider_flag = picker_flag = false;
		pulldown.draw();
	}

	// arduinoの終了処理
	if (serial_flag) {
		arduino.send(0);
		arduino.send(0);
		arduino.send(0);
		arduino.send(0);
		arduino.close();
	}
}
