document.getElementById("project-form").addEventListener("submit", async function(event) {
    event.preventDefault(); // Prevent default form submission

    // Get form field values
    const title = document.getElementById("title").value.trim();
    const description = document.getElementById("description").value.trim();
    const link = document.getElementById("link").value.trim();

    // Prepare data to send
    const postData = {
        title,
        description,
        link
    };

    try {
        const response = await fetch("/admin/project/create", {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify(postData)
        });

        if (response.ok) {
            // Handle success (e.g., show a success message or redirect)
            alert("Project added successfully!");
            document.getElementById("project-form").reset(); // Clear form
        } else {
            // Handle server error
            const errorData = await response.json();
            alert("Error: " + (errorData.message || "Something went wrong."));
        }
    } catch (error) {
        // Handle network error
        console.error("Fetch error:", error);
        alert("Network error. Please try again later.");
    }
});
