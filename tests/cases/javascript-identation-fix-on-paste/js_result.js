function main() {
  it("SPOT", async () => {
    const ei = await axios.get("http://127.0.0.1:7777/exchange-info/spot/btcusdt/1688202505000")
    expect(ei.status).toBe(200);
    expect(ei.data).toEqual(spotEi);
  });
}