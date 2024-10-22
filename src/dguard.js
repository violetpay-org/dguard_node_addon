const dguard = require("../build/Release/dguard.node")

dguard.start()

console.log("DGuard started")

dguard.encode("Hello, world!").then((result) => {
    console.log(result);
});
dguard.decode("Hello, world!").then((result) => {
    console.log(result);
});

setTimeout(() => {
    dguard.stop()

    console.log("DGuard stopped")
}, 6000)

