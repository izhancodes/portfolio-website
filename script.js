const roles = [
"Artificial Intelligence",
"Data Analytics",
"Software Development",
"SQL & Databases",
"Problem Solving"
];

let i = 0;

setInterval(() => {
document.getElementById("roles").innerText = roles[i];
i = (i + 1) % roles.length;
}, 1500);

document.getElementById("themeBtn").onclick = () => {
document.body.classList.toggle("light");
};

document.getElementById("askBtn").onclick = () => {
alert(
`Hi, I'm Izhan Shaikh

Computer Science Engineering Student

Exploring AI, Data Analytics & Software Development

Email: izhansshaikh2311@gmail.com
Phone: 9423131811`
);
};