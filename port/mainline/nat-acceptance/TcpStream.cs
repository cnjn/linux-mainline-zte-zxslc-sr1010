using System;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;

public static class TcpStream
{
	private static string Run(int milliseconds, int port, bool receive)
	{
		using (var client = new TcpClient(AddressFamily.InterNetwork)) {
			client.NoDelay = true;
			client.SendBufferSize = 4 * 1024 * 1024;
			client.ReceiveBufferSize = 4 * 1024 * 1024;
			client.Client.Bind(new IPEndPoint(
				IPAddress.Parse("192.168.5.100"), 0));
			client.Connect(IPAddress.Parse("192.168.1.100"), port);

			using (var stream = client.GetStream()) {
				var buffer = new byte[64 * 1024];
				var timer = Stopwatch.StartNew();
				long bytes = 0;

				while (timer.ElapsedMilliseconds < milliseconds) {
					if (receive) {
						int length = stream.Read(buffer, 0,
									 buffer.Length);
						if (length == 0)
							break;
						bytes += length;
					} else {
						stream.Write(buffer, 0, buffer.Length);
						bytes += buffer.Length;
					}
				}
				timer.Stop();

				return String.Format(
					"port={0} direction={1} bytes={2} " +
					"seconds={3:F6} payload_bps={4:F0}",
					port, receive ? "receive" : "send", bytes,
					timer.Elapsed.TotalSeconds,
					bytes * 8.0 / timer.Elapsed.TotalSeconds);
			}
		}
	}

	public static string Send(int milliseconds, int port)
	{
		return Run(milliseconds, port, false);
	}

	public static string Receive(int milliseconds, int port)
	{
		return Run(milliseconds, port, true);
	}
}
