using System;
using System.Diagnostics;
using System.Net;
using System.Net.Sockets;
using System.Threading;

public static class UdpPaced
{
	public static string Run(int milliseconds, int length, int port,
				 double targetLinkBps, int threads)
	{
		using (var socket = new Socket(AddressFamily.InterNetwork,
					       SocketType.Dgram, ProtocolType.Udp)) {
			socket.SendBufferSize = 4 * 1024 * 1024;
			socket.ReceiveBufferSize = 4 * 1024 * 1024;
			socket.Bind(new IPEndPoint(
				IPAddress.Parse("192.168.5.100"), port));
			socket.Connect(new IPEndPoint(
				IPAddress.Parse("192.168.1.100"), port));

			var gate = new ManualResetEvent(false);
			var ready = new CountdownEvent(threads);
			var workers = new Thread[threads];
			long packets = 0, bytes = 0, errors = 0;
			long receivedPackets = 0, receivedBytes = 0;
			int stopping = 0;
			double payloadBps = targetLinkBps * length / (length + 42.0);
			var receiver = new Thread(() => {
				var payload = new byte[2048];
				long localPackets = 0;
				long localBytes = 0;

				while (Volatile.Read(ref stopping) == 0) {
					if (!socket.Poll(100000, SelectMode.SelectRead))
						continue;
					try {
						localBytes += socket.Receive(payload);
						localPackets++;
					} catch (SocketException exception) {
						if (exception.SocketErrorCode !=
						    SocketError.ConnectionReset)
							throw;
					}
				}
				receivedPackets = localPackets;
				receivedBytes = localBytes;
			});
			receiver.Start();

			for (int i = 0; i < threads; i++) {
				workers[i] = new Thread(() => {
					var payload = new byte[length];
					long localPackets = 0;
					long localBytes = 0;
					long localErrors = 0;
					ready.Signal();
					gate.WaitOne();
					var timer = Stopwatch.StartNew();
					double share = payloadBps / threads;

					while (timer.ElapsedMilliseconds < milliseconds) {
						double allowed = timer.Elapsed.TotalSeconds *
								 share / 8.0;
						if (localBytes + length > allowed) {
							Thread.SpinWait(30);
							continue;
						}
						try {
							localBytes += socket.Send(payload);
							localPackets++;
						} catch (SocketException) {
							localErrors++;
						}
					}

					Interlocked.Add(ref packets, localPackets);
					Interlocked.Add(ref bytes, localBytes);
					Interlocked.Add(ref errors, localErrors);
				});
				workers[i].Start();
			}

			ready.Wait();
			var totalTimer = Stopwatch.StartNew();
			gate.Set();
			foreach (var worker in workers)
				worker.Join();
			Volatile.Write(ref stopping, 1);
			receiver.Join();
			totalTimer.Stop();

			return String.Format(
				"port={0} threads={1} packets={2} bytes={3} " +
				"errors={4} rx_packets={5} rx_bytes={6} " +
				"seconds={7:F6} payload_bps={8:F0}",
				port, threads, packets, bytes, errors,
				receivedPackets, receivedBytes,
				totalTimer.Elapsed.TotalSeconds,
				bytes * 8.0 / totalTimer.Elapsed.TotalSeconds);
		}
	}
}
