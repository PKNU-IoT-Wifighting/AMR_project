using GuideRobot.WebHmi.Components;
using GuideRobot.WebHmi.Services;

namespace GuideRobot.WebHmi
{
    public class Program
    {
        public static void Main(string[] args)
        {
            var builder = WebApplication.CreateBuilder(args);

            // Add services to the container.
            builder.Services.AddRazorComponents()
                .AddInteractiveServerComponents();

            var robotServerUrl = builder.Configuration["RobotServer:BaseUrl"]
                ?? throw new InvalidOperationException("RobotServer:BaseUrl 설정이 필요합니다.");

            builder.Services.AddHttpClient<RobotCommandService>(client =>
            {
                client.BaseAddress = new Uri(robotServerUrl);
                client.Timeout = TimeSpan.FromSeconds(10);
                // 무료 ngrok의 브라우저 경고 페이지 대신 API 응답을 받기 위한 헤더입니다.
                client.DefaultRequestHeaders.Add("ngrok-skip-browser-warning", "1");
            });

            var app = builder.Build();

            // Configure the HTTP request pipeline.
            if (!app.Environment.IsDevelopment())
            {
                app.UseExceptionHandler("/Error");
                // The default HSTS value is 30 days. You may want to change this for production scenarios, see https://aka.ms/aspnetcore-hsts.
                app.UseHsts();
            }

            app.UseStatusCodePagesWithReExecute("/not-found", createScopeForStatusCodePages: true);
            app.UseHttpsRedirection();

            app.UseAntiforgery();

            app.MapStaticAssets();
            app.MapRazorComponents<App>()
                .AddInteractiveServerRenderMode();

            app.Run();
        }
    }
}
